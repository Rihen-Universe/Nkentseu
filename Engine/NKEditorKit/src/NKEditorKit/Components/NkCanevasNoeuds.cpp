// -----------------------------------------------------------------------------
// FICHIER: NKEditorKit/Components/NkCanevasNoeuds.cpp
// DESCRIPTION: La toile nodale generique (NkCanevasNoeuds.h) : geometrie A LA
//              CHARTE de Rihen (References/Blueprint/CHARTE.md), gestes a la
//              UE5, dessin aux jetons de NkStyleNodal.h, trace d'execution.
//
// LA GEOMETRIE D'UN NOEUD (charte, planches 02, 03, 08) :
//   en-tete (titre, sous-titre) | FILET (orange : execute ; petrole : calcule)
//   | rangees de 24 : UNE prise par rangee -- d'abord les SORTIES D'EXECUTION
//   (le flot reste en haut, il se lit de gauche a droite), un ecart, les
//   entrees de donnees, le BLOC de corps du domaine (le code d'une
//   Expression), les sorties de donnees, et la raison d'une erreur, ECRITE.
//   La premiere ENTREE d'execution est sur l'EN-TETE (un evenement n'en a
//   pas : le bord gauche de son en-tete est NET, coin 12 px).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKEditorKit/Components/NkCanevasNoeuds.h"

#include "NKMath/NKMath.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			bool Dans(const NkRect &r, const NkVec2 &p) {
				return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
			}
			float32 Carre(float32 v) {
				return v * v;
			}
			float32 Min(float32 a, float32 b) {
				return a < b ? a : b;
			}
			float32 Max(float32 a, float32 b) {
				return a > b ? a : b;
			}
			float32 Absf(float32 v) {
				return v < 0.f ? -v : v;
			}
			const NkGeomNoeud *Geo(const NkEtatCanevas &e, graph::NkNodeId id) {
				for (uint32 i = 0; i < e.geom.Size(); ++i) {
					if (e.geom[i].id == id) {
						return &e.geom[i];
					}
				}
				return nullptr;
			}
			NkFormeNoeud FormeDe(const NkDomaineCanevas &d, const graph::NkNode &n) {
				return d.Forme != nullptr ? d.Forme(d.donnees, n) : NkFormeNoeud::NK_NORMAL;
			}
			void Texte(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 x, float32 top, const char *s, const NkColor &c,
					   float32 zoom, float32 maxW = -1.f) {
				if (f == nullptr || f->Face() == nullptr || s == nullptr || s[0] == '\0') {
					return;
				}
				const NkVec2 base{x, top + f->Ascent() * zoom};
				if (zoom > 0.97f && zoom < 1.03f) {
					dl.AddText(f->Face(), f->TexId(), base, s, c, maxW);
				} else {
					dl.AddTextScaled(f->Face(), f->TexId(), base, s, c, zoom, maxW);
				}
			}
			float32 Largeur(nkgui::NkGuiFont *f, const char *s, float32 zoom) {
				return f != nullptr && s != nullptr && f->Face() != nullptr ? f->MeasureWidth(s) * zoom : 0.f;
			}
			float32 HauteurLigne(nkgui::NkGuiFont *f) {
				return f != nullptr && f->Face() != nullptr ? f->LineHeight() : 14.f;
			}
			const char *NomType(const graph::NkNodeGraph &g, const graph::NkSocket &s) {
				const NkString *t = g.TypeName(s.type);
				return t != nullptr ? t->CStr() : "?";
			}
			NkColor CouleurPrise(const graph::NkNodeGraph &g, const graph::NkSocket &s, const NkStyleCanevas &st,
								 const NkDomaineCanevas &d) {
				if (s.family == graph::NkSocketFamily::Exec) {
					return st.exec; // TOUJOURS orange
				}
				if (d.CouleurType != nullptr) {
					return d.CouleurType(d.donnees, g, s.type);
				}
				return NkCouleurTypeNodal(NomType(g, s));
			}
			NkString LibellePrise(const graph::NkNodeGraph &g, const graph::NkNode &n, const graph::NkSocket &s,
								  const NkDomaineCanevas &d) {
				if (d.LibellePrise != nullptr) {
					return d.LibellePrise(d.donnees, g, n, s);
				}
				if (s.family == graph::NkSocketFamily::Exec) {
					if (s.name == "exec") {
						return NkString();
					}
					if (s.name == "suite") {
						return NkString("Terminé");
					}
				}
				return s.name;
			}
			bool PriseLiee(const graph::NkNodeGraph &g, graph::NkNodeId n, int32 k) {
				for (uint32 i = 0; i < g.LinkCount(); ++i) {
					const graph::NkLink *l = g.LinkAt(i);
					if (l != nullptr && l->alive &&
						((l->fromNode == n && l->fromSocket == k) || (l->toNode == n && l->toSocket == k))) {
						return true;
					}
				}
				return false;
			}
			bool Supprimable(const NkDomaineCanevas &d, const graph::NkNode &n) {
				return d.Supprimable == nullptr || d.Supprimable(d.donnees, n);
			}
			/// Un noeud qui EXECUTE (filet orange) : il porte une prise d'execution.
			bool Execute(const graph::NkNode &n) {
				for (uint32 k = 0; k < n.sockets.Size(); ++k) {
					if (n.sockets[k].family == graph::NkSocketFamily::Exec) {
						return true;
					}
				}
				return false;
			}
			/// L'indice de la premiere ENTREE d'execution (sur l'en-tete), ou -1.
			int32 ExecEnTete(const graph::NkNode &n) {
				for (uint32 k = 0; k < n.sockets.Size(); ++k) {
					if (n.sockets[k].family == graph::NkSocketFamily::Exec && n.sockets[k].dir == graph::NkSocketDir::Input) {
						return static_cast<int32>(k);
					}
				}
				return -1;
			}
			/// Les points de controle de la cubique d'un fil, de `a` (sortie) a `b` (entree).
			void Controles(const NkVec2 &a, const NkVec2 &b, NkVec2 &c1, NkVec2 &c2) {
				const float32 dx = Absf(b.x - a.x) * 0.5f;
				const float32 tire = dx < 40.f ? 40.f : dx;
				c1 = NkVec2{a.x + tire, a.y};
				c2 = NkVec2{b.x - tire, b.y};
			}
			/// Une cubique de `a` (sortie) a `b` (entree), echantillonnee en `nb` points.
			void Courbe(const NkVec2 &a, const NkVec2 &b, NkVec2 *pts, int32 nb) {
				NkVec2 c1, c2;
				Controles(a, b, c1, c2);
				for (int32 i = 0; i < nb; ++i) {
					pts[i] = nkgui::NkGuiDrawList::PointBezier(a, c1, c2, b, static_cast<float32>(i) / static_cast<float32>(nb - 1));
				}
			}
			/// Le nombre de points d'un fil A L'ECRAN : il suit sa longueur (NKGui),
			/// pour qu'aucune courbe ne soit anguleuse (au plus 257 : `kPointsFil`).
			constexpr int32 kPointsFil = 257;
			int32 PointsFil(const NkVec2 &a, const NkVec2 &b) {
				NkVec2 c1, c2;
				Controles(a, b, c1, c2);
				return nkgui::NkGuiDrawList::SegmentsBezier(a, c1, c2, b) + 1;
			}
			/// Un fil : la cubique LISSEE de NKGui (frange, segments adaptatifs, bouts ronds).
			void Fil(nkgui::NkGuiDrawList &dl, const NkVec2 &a, const NkVec2 &b, const NkColor &c, float32 epaisseur) {
				NkVec2 c1, c2;
				Controles(a, b, c1, c2);
				dl.AddBezierCubic(a, c1, c2, b, c, epaisseur, 0, true);
			}
			/// Une polyligne en POINTILLE (tirets `plein` / `vide`, en pixels).
			void Pointille(nkgui::NkGuiDrawList &dl, const NkVec2 *pts, int32 n, const NkColor &c, float32 ep, float32 plein, float32 vide) {
				// Chaque TIRET est une petite polyligne LISSE (ses points suivent la
				// courbe) : pas de quads separes qui se chevauchent a ses sommets.
				float32 phase = 0.f;
				bool trace = true;
				NkVec2 tiret[64];
				int32 nt = 0;
				auto vider = [&]() {
					if (nt >= 2) {
						dl.AddPolyline(tiret, nt, c, ep); // lisse si oblique, net si droit
					}
					nt = 0;
				};
				for (int32 i = 0; i + 1 < n; ++i) {
					NkVec2 a = pts[i];
					const NkVec2 b = pts[i + 1];
					float32 lg = math::NkSqrt(Carre(b.x - a.x) + Carre(b.y - a.y));
					while (lg > 1e-3f) {
						const float32 reste = (trace ? plein : vide) - phase;
						const float32 pas = reste < lg ? reste : lg;
						const float32 t = pas / lg;
						const NkVec2 m{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
						if (trace) {
							if (nt == 0) {
								tiret[nt++] = a;
							}
							if (nt < 64) {
								tiret[nt++] = m;
							}
						}
						phase += pas;
						if (phase >= (trace ? plein : vide) - 1e-3f) {
							phase = 0.f;
							if (trace) {
								vider();
							}
							trace = !trace;
						}
						a = m;
						lg -= pas;
					}
				}
				vider();
			}
			void RectPointille(nkgui::NkGuiDrawList &dl, const NkRect &r, const NkColor &c, float32 ep, float32 tiret) {
				const NkVec2 pts[5] = {{r.x, r.y}, {r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}, {r.x, r.y}};
				Pointille(dl, pts, 5, c, ep, tiret, tiret);
			}
			float32 DistanceAuFil(const NkVec2 &a, const NkVec2 &b, const NkVec2 &p) {
				NkVec2 pts[25];
				Courbe(a, b, pts, 25);
				float32 mieux = 1e30f;
				for (int32 i = 0; i + 1 < 25; ++i) {
					const NkVec2 u = pts[i], v = pts[i + 1];
					const float32 dx = v.x - u.x, dy = v.y - u.y;
					const float32 l2 = dx * dx + dy * dy;
					float32 t = l2 > 1e-6f ? ((p.x - u.x) * dx + (p.y - u.y) * dy) / l2 : 0.f;
					t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
					const float32 d2 = Carre(u.x + t * dx - p.x) + Carre(u.y + t * dy - p.y);
					mieux = d2 < mieux ? d2 : mieux;
				}
				return mieux;
			}
			/// La prise d'EXECUTION : un rectangle a POINTE (pointe a 58 % de la
			/// largeur), de `x0` (gauche) sur `w`, centre en `cy`. Creuse = non
			/// branchee, pleine = branchee.
			/// Un petit triangle (fleche, repli) au bord ADOUCI (NKGui).
			void TriangleLisse(nkgui::NkGuiDrawList &dl, const NkVec2 &a, const NkVec2 &b, const NkVec2 &c, const NkColor &col) {
				const NkVec2 t[3] = {a, b, c};
				dl.AddConvexPolyLisse(t, 3, col);
			}
			void PriseExec(nkgui::NkGuiDrawList &dl, float32 x0, float32 cy, float32 w, float32 h, const NkColor &c, const NkColor &fond,
						   bool pleine) {
				const float32 xp = x0 + w * 0.58f;
				const NkVec2 p[5] = {{x0, cy - h * 0.5f}, {xp, cy - h * 0.5f}, {x0 + w, cy}, {xp, cy + h * 0.5f}, {x0, cy + h * 0.5f}};
				dl.AddConvexPolyLisse(p, 5, pleine ? c : fond); // la pointe adoucie
				if (!pleine) {
					dl.AddPolyline(p, 5, c, 1.5f, true);
				}
			}
			/// La prise de DONNEE : une languette, entierement dehors.
			void PriseDonnee(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, float32 w, float32 h, const NkColor &c, const NkColor &fond,
							 bool pleine, float32 contour) {
				const NkRect r{cx - w * 0.5f, cy - h * 0.5f, w, h};
				if (pleine) {
					dl.AddRectFilled(r, c, Min(w * 0.45f, 2.f));
				} else {
					dl.AddRectFilled(r, fond, Min(w * 0.45f, 2.f));
					dl.AddRect(r, c, contour, Min(w * 0.45f, 2.f));
				}
			}
			/// La valeur d'une entree LIBRE a montrer (vide : pas de champ).
			NkString ValeurLibre(const graph::NkNodeGraph &g, const graph::NkNode &n, uint32 k, const NkDomaineCanevas &d) {
				const graph::NkSocket &s = n.sockets[k];
				if (s.dir != graph::NkSocketDir::Input || s.family != graph::NkSocketFamily::Data || d.TexteDefaut == nullptr ||
					PriseLiee(g, n.id, static_cast<int32>(k))) {
					return NkString();
				}
				return d.TexteDefaut(d.donnees, g, n, s);
			}
			NkGenreChamp GenreChamp(const graph::NkNodeGraph &g, const graph::NkNode &n, const graph::NkSocket &s, const NkDomaineCanevas &d) {
				return d.Champ != nullptr ? d.Champ(d.donnees, g, n, s) : NkGenreChamp::NK_TEXTE;
			}
			float32 LargeurPastille(nkgui::NkGuiFont *f, const char *glyphe) {
				return Largeur(f, glyphe, 0.72f) + 8.f;
			}
			/// La pastille de TYPE : le glyphe, sur la couleur du type a 22 %.
			void Pastille(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 x, float32 cy, const char *glyphe, const NkColor &c,
						  const NkStyleCanevas &s, float32 z, float32 alpha) {
				const float32 w = LargeurPastille(f, glyphe) * z, h = 13.f * z;
				const NkRect r{x, cy - h * 0.5f, w, h};
				dl.AddRectFilled(r, NkAlphaNodal(c, s.pastilleAlpha * alpha), 2.f * z);
				Texte(dl, f, x + 4.f * z, cy - HauteurLigne(f) * 0.72f * z * 0.5f, glyphe, NkAlphaNodal(c, alpha), z * 0.72f);
			}
		} // namespace

		NkVec2 NkCanevasVersEcran(const NkEtatCanevas &e, const NkRect &zone, float32 x, float32 y) noexcept {
			return NkVec2{zone.x + (x - e.vueX) * e.zoom, zone.y + (y - e.vueY) * e.zoom};
		}

		NkVec2 NkCanevasDepuisEcran(const NkEtatCanevas &e, const NkRect &zone, const NkVec2 &p) noexcept {
			return NkVec2{e.vueX + (p.x - zone.x) / e.zoom, e.vueY + (p.y - zone.y) / e.zoom};
		}

		namespace {
			/// La hauteur (graphe) des prises d'un noeud SANS geometrie mesuree :
			/// la meme regle que Mesurer, sans police ni domaine.
			float32 YPriseParDefaut(const graph::NkNode &n, int32 k, const NkStyleCanevas &s) {
				const int32 tete = ExecEnTete(n);
				if (k == tete) {
					return s.hauteurEntete * 0.5f;
				}
				float32 y = s.hauteurEntete + s.filet + 4.f;
				for (int32 passe = 0; passe < 4; ++passe) {
					for (uint32 i = 0; i < n.sockets.Size(); ++i) {
						const graph::NkSocket &so = n.sockets[i];
						const bool exec = so.family == graph::NkSocketFamily::Exec;
						const bool entree = so.dir == graph::NkSocketDir::Input;
						const int32 groupe = exec ? (entree ? 0 : 1) : (entree ? 2 : 3);
						if (groupe != passe || static_cast<int32>(i) == tete) {
							continue;
						}
						if (static_cast<int32>(i) == k) {
							return y + s.hauteurRangee * 0.5f;
						}
						y += s.hauteurRangee;
					}
				}
				return y;
			}
		} // namespace

		NkRect NkCanevasRectNoeud(const NkEtatCanevas &e, const NkRect &zone, const graph::NkNode &n, const NkStyleCanevas &s) noexcept {
			const NkVec2 p = NkCanevasVersEcran(e, zone, n.x, n.y);
			if (const NkGeomNoeud *g = Geo(e, n.id)) {
				return NkRect{p.x, p.y, g->w * e.zoom, g->h * e.zoom};
			}
			const float32 h = YPriseParDefaut(n, static_cast<int32>(n.sockets.Size()), s) + 8.f;
			return NkRect{p.x, p.y, s.largeurMin * e.zoom, h * e.zoom};
		}

		bool NkCanevasPrise(const NkEtatCanevas &e, const NkRect &zone, const graph::NkNode &n, int32 k, const NkStyleCanevas &s,
							NkVec2 &sortie) noexcept {
			if (k < 0 || static_cast<uint32>(k) >= n.sockets.Size()) {
				return false;
			}
			const NkRect r = NkCanevasRectNoeud(e, zone, n, s);
			const NkGeomNoeud *g = Geo(e, n.id);
			const NkFormeNoeud forme = g != nullptr ? g->forme : NkFormeNoeud::NK_NORMAL;
			const graph::NkSocket &so = n.sockets[static_cast<uint32>(k)];
			const bool entree = so.dir == graph::NkSocketDir::Input;
			const float32 z = e.zoom;
			if (forme == NkFormeNoeud::NK_RELAIS) {
				sortie = NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
				return true;
			}
			if (forme == NkFormeNoeud::NK_COMPACT) {
				sortie = NkVec2{entree ? r.x - s.priseLargeur * 0.5f * z : r.x + r.w + s.priseLargeur * 0.5f * z, r.y + r.h * 0.5f};
				return true;
			}
			const float32 y = g != nullptr && static_cast<uint32>(k) < g->yPrises.Size() ? g->yPrises[static_cast<uint32>(k)]
																						 : YPriseParDefaut(n, k, s);
			if (so.family == graph::NkSocketFamily::Exec) {
				// Le fil d'execution part de la POINTE (sortie) et arrive au DOS (entree).
				sortie = NkVec2{entree ? r.x - s.execDehors * z : r.x + r.w + s.execDehors * z, r.y + y * z};
			} else {
				// Le fil de donnee part du CENTRE de la languette, dehors.
				sortie = NkVec2{entree ? r.x - s.priseLargeur * 0.5f * z : r.x + r.w + s.priseLargeur * 0.5f * z, r.y + y * z};
			}
			return true;
		}

		void NkCanevasCadrer(NkEtatCanevas &e, const NkRect &zone, const graph::NkNodeGraph &g, const NkStyleCanevas &s) noexcept {
			float32 x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
			for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
				const graph::NkNode *n = g.RawNodeAt(i);
				if (n == nullptr || !n->alive) {
					continue;
				}
				const NkGeomNoeud *gg = Geo(e, n->id);
				const float32 w = gg != nullptr ? gg->w : s.largeurMin;
				const float32 h = gg != nullptr ? gg->h : YPriseParDefaut(*n, static_cast<int32>(n->sockets.Size()), s) + 8.f;
				x0 = n->x < x0 ? n->x : x0;
				y0 = n->y < y0 ? n->y : y0;
				x1 = n->x + w > x1 ? n->x + w : x1;
				y1 = n->y + h > y1 ? n->y + h : y1;
			}
			if (x1 < x0) {
				e.vueX = -40.f;
				e.vueY = -40.f;
				e.zoom = 1.f;
				return;
			}
			const float32 marge = 56.f;
			float32 z = (zone.w - 2.f * marge) / (x1 - x0);
			const float32 zy = (zone.h - 2.f * marge) / (y1 - y0);
			z = zy < z ? zy : z;
			// Lisible d'abord : en dessous de 0,7 le texte des noeuds ne se lit plus ;
			// au-dessus de 1 un petit graphe parait gonfle (la charte est dessinee a 1).
			z = z < 0.7f ? 0.7f : (z > 1.f ? 1.f : z);
			e.zoom = z;
			const float32 libreX = zone.w / z - (x1 - x0);
			const float32 libreY = zone.h / z - (y1 - y0);
			e.vueX = libreX > 0.f ? x0 - libreX * 0.5f : x0 - marge / z;
			e.vueY = libreY > 0.f ? y0 - libreY * 0.5f : y0 - marge / z;
		}

		// =====================================================================
		// Copier, coller, supprimer, aligner
		// =====================================================================
		namespace {
			/// Le type `t` de `de`, dans `vers` (enregistre s'il manque).
			graph::NkTypeId Traduire(const graph::NkNodeGraph &de, graph::NkNodeGraph &vers, graph::NkTypeId t) {
				const NkString *nom = de.TypeName(t);
				if (nom == nullptr) {
					return graph::NK_TYPE_INVALID;
				}
				const graph::NkTypeId v = vers.FindType(nom->CStr());
				return v != graph::NK_TYPE_INVALID ? v : vers.RegisterType(nom->CStr());
			}
			graph::NkGraphValue TraduireValeur(const graph::NkNodeGraph &de, graph::NkNodeGraph &vers, const graph::NkGraphValue &v) {
				graph::NkGraphValue r = v;
				if (v.IsSet()) {
					r.type = Traduire(de, vers, v.type);
				}
				return r;
			}
			/// Copie le noeud `n` de `de` dans `vers` ; rend son identifiant.
			graph::NkNodeId CopierNoeud(const graph::NkNodeGraph &de, graph::NkNodeGraph &vers, const graph::NkNode &n, float32 dx,
										float32 dy) {
				const graph::NkNodeId c = vers.AddNode(n.type.CStr(), n.label.CStr());
				for (uint32 k = 0; k < n.sockets.Size(); ++k) {
					const graph::NkSocket &s = n.sockets[k];
					vers.AddSocket(c, s.name.CStr(), Traduire(de, vers, s.type), s.dir, s.family);
					if (s.defaultValue.IsSet()) {
						vers.SetSocketDefault(c, s.name.CStr(), s.dir, TraduireValeur(de, vers, s.defaultValue));
					}
				}
				for (uint32 k = 0; k < n.props.Size(); ++k) {
					vers.SetProp(c, n.props[k].name.CStr(), TraduireValeur(de, vers, n.props[k].value));
				}
				if (graph::NkNode *p = vers.Find(c)) {
					p->x = n.x + dx;
					p->y = n.y + dy;
					p->subgraph = n.subgraph;
				}
				return c;
			}
		} // namespace

		NkString NkCanevasCopier(const graph::NkNodeGraph &g, const NkEtatCanevas &e, const NkDomaineCanevas &d) {
			graph::NkNodeGraph tmp;
			// Les conversions du graphe : un fil accepte la-bas l'est ici.
			for (uint32 i = 0; i < g.ConversionCount(); ++i) {
				graph::NkTypeId a = 0, b = 0;
				if (g.ConversionAt(i, &a, &b)) {
					tmp.AllowConversion(Traduire(g, tmp, a), Traduire(g, tmp, b));
				}
			}
			NkVector<graph::NkNodeId> de, vers;
			for (uint32 i = 0; i < e.choix.Size(); ++i) {
				const graph::NkNode *n = g.Find(e.choix[i]);
				if (n == nullptr || !Supprimable(d, *n)) {
					continue;
				}
				de.PushBack(n->id);
				vers.PushBack(CopierNoeud(g, tmp, *n, 0.f, 0.f));
			}
			if (de.Empty()) {
				return NkString();
			}
			for (uint32 i = 0; i < g.LinkCount(); ++i) {
				const graph::NkLink *l = g.LinkAt(i);
				if (l == nullptr || !l->alive) {
					continue;
				}
				int32 a = -1, b = -1;
				for (uint32 k = 0; k < de.Size(); ++k) {
					a = de[k] == l->fromNode ? static_cast<int32>(k) : a;
					b = de[k] == l->toNode ? static_cast<int32>(k) : b;
				}
				if (a < 0 || b < 0) {
					continue; // un fil vers l'exterieur ne se copie pas
				}
				const graph::NkNode *na = g.Find(l->fromNode);
				const graph::NkNode *nb = g.Find(l->toNode);
				tmp.Connect(vers[static_cast<uint32>(a)], na->sockets[static_cast<uint32>(l->fromSocket)].name.CStr(), vers[static_cast<uint32>(b)],
							nb->sockets[static_cast<uint32>(l->toSocket)].name.CStr());
			}
			NkString s;
			tmp.Serialize(s);
			return s;
		}

		uint32 NkCanevasColler(graph::NkNodeGraph &g, NkEtatCanevas &e, const char *texte, float32 x, float32 y) {
			if (texte == nullptr || texte[0] == '\0') {
				return 0u;
			}
			graph::NkNodeGraph tmp;
			if (!tmp.Deserialize(texte)) {
				return 0u;
			}
			float32 x0 = 1e30f, y0 = 1e30f;
			for (uint32 i = 0; i < tmp.RawNodeCount(); ++i) {
				const graph::NkNode *n = tmp.RawNodeAt(i);
				if (n != nullptr && n->alive) {
					x0 = n->x < x0 ? n->x : x0;
					y0 = n->y < y0 ? n->y : y0;
				}
			}
			if (x0 > 1e29f) {
				return 0u;
			}
			NkVector<graph::NkNodeId> de, vers;
			for (uint32 i = 0; i < tmp.RawNodeCount(); ++i) {
				const graph::NkNode *n = tmp.RawNodeAt(i);
				if (n != nullptr && n->alive) {
					de.PushBack(n->id);
					vers.PushBack(CopierNoeud(tmp, g, *n, x - x0, y - y0));
				}
			}
			for (uint32 i = 0; i < tmp.LinkCount(); ++i) {
				const graph::NkLink *l = tmp.LinkAt(i);
				if (l == nullptr || !l->alive) {
					continue;
				}
				int32 a = -1, b = -1;
				for (uint32 k = 0; k < de.Size(); ++k) {
					a = de[k] == l->fromNode ? static_cast<int32>(k) : a;
					b = de[k] == l->toNode ? static_cast<int32>(k) : b;
				}
				if (a >= 0 && b >= 0) {
					const graph::NkNode *na = tmp.Find(l->fromNode);
					const graph::NkNode *nb = tmp.Find(l->toNode);
					g.Connect(vers[static_cast<uint32>(a)], na->sockets[static_cast<uint32>(l->fromSocket)].name.CStr(),
							  vers[static_cast<uint32>(b)], nb->sockets[static_cast<uint32>(l->toSocket)].name.CStr());
				}
			}
			e.choix = vers;
			e.selection = vers.Empty() ? graph::NK_NODE_INVALID : vers.Back();
			e.modifie = true;
			return static_cast<uint32>(vers.Size());
		}

		uint32 NkCanevasSupprimerChoix(graph::NkNodeGraph &g, NkEtatCanevas &e, const NkDomaineCanevas &d) {
			uint32 n = 0;
			NkVector<graph::NkNodeId> restent;
			for (uint32 i = 0; i < e.choix.Size(); ++i) {
				const graph::NkNode *p = g.Find(e.choix[i]);
				if (p == nullptr) {
					continue;
				}
				if (!Supprimable(d, *p)) {
					restent.PushBack(p->id);
					continue;
				}
				g.RemoveNode(e.choix[i]);
				++n;
			}
			e.choix = restent;
			e.selection = restent.Empty() ? graph::NK_NODE_INVALID : restent[0];
			if (n > 0u) {
				e.modifie = true;
			}
			return n;
		}

		bool NkCanevasBoiteChoix(const graph::NkNodeGraph &g, const NkEtatCanevas &e, const NkStyleCanevas &s, float32 &x, float32 &y,
								 float32 &w, float32 &h) {
			float32 x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
			for (uint32 i = 0; i < e.choix.Size(); ++i) {
				const graph::NkNode *n = g.Find(e.choix[i]);
				if (n == nullptr) {
					continue;
				}
				const NkGeomNoeud *gg = Geo(e, n->id);
				const float32 nw = gg != nullptr ? gg->w : s.largeurMin;
				const float32 nh = gg != nullptr ? gg->h : 60.f;
				x0 = Min(x0, n->x);
				y0 = Min(y0, n->y);
				x1 = Max(x1, n->x + nw);
				y1 = Max(y1, n->y + nh);
			}
			if (x1 < x0) {
				return false;
			}
			const float32 m = 24.f;
			x = x0 - m;
			y = y0 - m - s.hauteurEnteteCommentaire;
			w = x1 - x0 + 2.f * m;
			h = y1 - y0 + 2.f * m + s.hauteurEnteteCommentaire;
			return true;
		}

		void NkCanevasAligner(graph::NkNodeGraph &g, NkEtatCanevas &e, int32 mode) {
			if (e.choix.Size() < 2u) {
				return;
			}
			float32 ref = 0.f;
			bool premier = true;
			for (uint32 i = 0; i < e.choix.Size(); ++i) {
				const graph::NkNode *n = g.Find(e.choix[i]);
				if (n == nullptr) {
					continue;
				}
				const NkGeomNoeud *gg = Geo(e, n->id);
				const float32 w = gg != nullptr ? gg->w : 0.f, h = gg != nullptr ? gg->h : 0.f;
				float32 v = 0.f;
				switch (mode) {
					case 0:
						v = n->x;
						ref = premier ? v : Min(ref, v);
						break;
					case 1:
						v = n->x + w;
						ref = premier ? v : Max(ref, v);
						break;
					case 2:
						v = n->y;
						ref = premier ? v : Min(ref, v);
						break;
					case 3:
						v = n->y + h;
						ref = premier ? v : Max(ref, v);
						break;
					case 4:
						v = n->x + w * 0.5f;
						ref = premier ? v : ref + v;
						break;
					default:
						v = n->y + h * 0.5f;
						ref = premier ? v : ref + v;
						break;
				}
				premier = false;
			}
			if (mode >= 4) {
				ref /= static_cast<float32>(e.choix.Size());
			}
			for (uint32 i = 0; i < e.choix.Size(); ++i) {
				graph::NkNode *n = g.Find(e.choix[i]);
				if (n == nullptr) {
					continue;
				}
				const NkGeomNoeud *gg = Geo(e, n->id);
				const float32 w = gg != nullptr ? gg->w : 0.f, h = gg != nullptr ? gg->h : 0.f;
				switch (mode) {
					case 0:
						n->x = ref;
						break;
					case 1:
						n->x = ref - w;
						break;
					case 2:
						n->y = ref;
						break;
					case 3:
						n->y = ref - h;
						break;
					case 4:
						n->x = ref - w * 0.5f;
						break;
					default:
						n->y = ref - h * 0.5f;
						break;
				}
			}
			e.modifie = true;
		}

		void NkCanevasRedresser(graph::NkNodeGraph &g, NkEtatCanevas &e, const NkStyleCanevas &s) {
			const NkRect zone{0.f, 0.f, 1.f, 1.f};
			NkEtatCanevas unite = e; // les positions de prises a zoom 1, vue 0
			unite.vueX = 0.f;
			unite.vueY = 0.f;
			unite.zoom = 1.f;
			for (uint32 i = 0; i < e.choix.Size(); ++i) {
				graph::NkNode *n = g.Find(e.choix[i]);
				if (n == nullptr) {
					continue;
				}
				// La premiere entree reliee (d'execution d'abord) donne la hauteur.
				const graph::NkLink *mieux = nullptr;
				for (uint32 k = 0; k < g.LinkCount(); ++k) {
					const graph::NkLink *l = g.LinkAt(k);
					if (l == nullptr || !l->alive || l->toNode != n->id) {
						continue;
					}
					const bool exec = n->sockets[static_cast<uint32>(l->toSocket)].family == graph::NkSocketFamily::Exec;
					if (mieux == nullptr || exec) {
						mieux = l;
					}
					if (exec) {
						break;
					}
				}
				if (mieux == nullptr) {
					continue;
				}
				const graph::NkNode *src = g.Find(mieux->fromNode);
				NkVec2 a, b;
				if (src != nullptr && NkCanevasPrise(unite, zone, *src, mieux->fromSocket, s, a) &&
					NkCanevasPrise(unite, zone, *n, mieux->toSocket, s, b)) {
					n->y += a.y - b.y;
					e.modifie = true;
				}
			}
		}


		// =====================================================================
		// La trame : geometrie, gestes, dessin
		// =====================================================================
		namespace {
			/// La geometrie de tous les noeuds, mesuree a la police (unites du graphe).
			void Mesurer(NkEtatCanevas &e, const graph::NkNodeGraph &g, nkgui::NkGuiFont *f, const NkStyleCanevas &s,
						 const NkDomaineCanevas &d, bool trace) {
				e.geom.Clear();
				for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
					const graph::NkNode *n = g.RawNodeAt(i);
					if (n == nullptr || !n->alive) {
						continue;
					}
					NkGeomNoeud geo;
					geo.id = n->id;
					geo.forme = FormeDe(d, *n);
					switch (geo.forme) {
						case NkFormeNoeud::NK_RELAIS:
							geo.w = geo.h = s.rayonRelais * 2.f;
							break;
						case NkFormeNoeud::NK_COMMENTAIRE: {
							float32 w = 320.f, h = 180.f;
							if (d.TailleCommentaire != nullptr) {
								d.TailleCommentaire(d.donnees, g, *n, w, h);
							}
							geo.w = w;
							geo.h = h;
							break;
						}
						case NkFormeNoeud::NK_COMPACT: {
							// La puce : bloc-icone (le glyphe du type), libelle, une sortie.
							const float32 w = 30.f + Largeur(f, n->label.CStr(), 1.f) + 24.f;
							geo.w = w < 110.f ? 110.f : w;
							geo.h = 26.f;
							break;
						}
						default: {
							const NkString st = d.SousTitre != nullptr ? d.SousTitre(d.donnees, g, *n) : NkString();
							geo.entete = st.Empty() ? s.hauteurEntete : s.hauteurEnteteDouble;
							float32 w = 22.f + Largeur(f, n->label.Empty() ? n->type.CStr() : n->label.CStr(), 1.f) + 30.f + (trace ? 40.f : 0.f);
							w = Max(w, 22.f + Largeur(f, st.CStr(), 0.8f) + 20.f);
							// La largeur minimale AVANT le bloc : il se coupe a cette largeur
							// (sinon un bloc de code se mesurait etroit, donc trop haut).
							if (d.LargeurMin != nullptr) {
								w = Max(w, d.LargeurMin(d.donnees, *n));
							}
							w = Max(w, s.largeurMin);
							// Les rangees, une prise chacune.
							geo.yPrises.Resize(n->sockets.Size(), 0.f);
							const int32 tete = ExecEnTete(*n);
							if (tete >= 0) {
								geo.yPrises[static_cast<uint32>(tete)] = s.hauteurEntete * 0.5f;
							}
							float32 y = geo.entete + s.filet + 4.f;
							bool flot = false;
							for (int32 passe = 0; passe < 4; ++passe) {
								if (passe == 2 && flot) {
									y += s.ecartGroupes; // l'execution en haut, puis les donnees
								}
								if (passe == 3) {
									// Le BLOC du domaine, entre les entrees et les sorties.
									const float32 hb = d.HauteurBloc != nullptr ? d.HauteurBloc(d.donnees, g, *n, w) : 0.f;
									if (hb > 0.f) {
										geo.blocY = y + 2.f;
										geo.blocH = hb;
										y += hb + 4.f;
									}
								}
								for (uint32 k = 0; k < n->sockets.Size(); ++k) {
									const graph::NkSocket &so = n->sockets[k];
									const bool exec = so.family == graph::NkSocketFamily::Exec;
									const bool entree = so.dir == graph::NkSocketDir::Input;
									const int32 groupe = exec ? (entree ? 0 : 1) : (entree ? 2 : 3);
									if (groupe != passe || static_cast<int32>(k) == tete) {
										continue;
									}
									flot = flot || exec;
									geo.yPrises[k] = y + s.hauteurRangee * 0.5f;
									y += s.hauteurRangee;
									// La largeur de la rangee : pastille, libelle, champ.
									const NkString lib = LibellePrise(g, *n, so, d);
									float32 lw = 14.f + Largeur(f, lib.CStr(), 1.f);
									if (!exec) {
										lw += LargeurPastille(f, NkGlypheTypeNodal(NomType(g, so))) + 6.f;
									}
									if (entree && !exec) {
										const NkGenreChamp gc = GenreChamp(g, *n, so, d);
										lw += gc == NkGenreChamp::NK_CASE ? 26.f : (gc == NkGenreChamp::NK_AUCUN ? 0.f : s.largeurChamp + 14.f);
									}
									w = Max(w, lw + 18.f);
								}
							}
							if (d.LargeurMin != nullptr) {
								w = Max(w, d.LargeurMin(d.donnees, *n));
							}
							geo.w = w < s.largeurMin ? s.largeurMin : w;
							// La raison d'une erreur, ECRITE dans le noeud.
							if (d.Erreur != nullptr) {
								geo.erreur = d.Erreur(d.donnees, g, *n);
							}
							if (geo.erreur.Empty() && n->id == e.enErreur) {
								geo.erreur = e.messageErreur;
							}
							if (!geo.erreur.Empty()) {
								geo.erreurY = y + 2.f;
								const float32 lignes = Largeur(f, geo.erreur.CStr(), 0.86f) > geo.w - 24.f ? 2.f : 1.f;
								y += 6.f + lignes * HauteurLigne(f) * 0.9f;
							}
							geo.h = y + 8.f;
							break;
						}
					}
					e.geom.PushBack(geo);
				}
			}

			/// Ce qui est sous un point de l'ecran.
			struct Cible {
					graph::NkNodeId noeud = graph::NK_NODE_INVALID;
					int32 prise = -1;
					graph::NkNodeId champNoeud = graph::NK_NODE_INVALID;
					int32 champ = -1;
					graph::NkNodeId bloc = graph::NK_NODE_INVALID; ///< le bloc de corps (au domaine)
					graph::NkNodeId commentaire = graph::NK_NODE_INVALID;
					bool enteteCommentaire = false;
					bool poignee = false; ///< le coin bas-droit d'un cadre
			};

			/// Le champ d'une entree libre (ECRAN), aligne a DROITE de sa rangee.
			NkRect RectChamp(const NkEtatCanevas &e, const NkRect &r, const graph::NkNode &n, uint32 k, const NkStyleCanevas &s,
							 NkGenreChamp gc) {
				const NkGeomNoeud *geo = Geo(e, n.id);
				if (geo == nullptr || k >= geo->yPrises.Size()) {
					return NkRect{0.f, 0.f, 0.f, 0.f};
				}
				const float32 z = e.zoom;
				const float32 cy = r.y + geo->yPrises[k] * z;
				if (gc == NkGenreChamp::NK_CASE) {
					const float32 c = 13.f * z;
					return NkRect{r.x + r.w - s.margeNoeud * z - c, cy - c * 0.5f, c, c};
				}
				const float32 w = s.largeurChamp * z, h = s.hauteurChamp * z;
				return NkRect{r.x + r.w - s.margeNoeud * z - w, cy - h * 0.5f, w, h};
			}

			Cible Viser(const NkEtatCanevas &e, const NkRect &zone, const graph::NkNodeGraph &g, const NkVec2 &souris, const NkStyleCanevas &s,
						const NkDomaineCanevas &d) {
				Cible c;
				const float32 z = e.zoom;
				const float32 tol = Max(7.f, 9.f * z);
				// Du dernier dessine au premier : les noeuds, puis les cadres (dessous).
				for (uint32 i = g.RawNodeCount(); i-- > 0u;) {
					const graph::NkNode *n = g.RawNodeAt(i);
					if (n == nullptr || !n->alive) {
						continue;
					}
					const NkGeomNoeud *geo = Geo(e, n->id);
					if (geo != nullptr && geo->forme == NkFormeNoeud::NK_COMMENTAIRE) {
						continue;
					}
					const NkRect r = NkCanevasRectNoeud(e, zone, *n, s);
					for (uint32 k = 0; k < n->sockets.Size(); ++k) {
						NkVec2 p;
						if (!NkCanevasPrise(e, zone, *n, static_cast<int32>(k), s, p)) {
							continue;
						}
						if (geo != nullptr && geo->forme == NkFormeNoeud::NK_RELAIS) {
							// Un relais : son entree a gauche du centre, sa sortie a droite.
							if (Carre(p.x - souris.x) + Carre(p.y - souris.y) <= Carre(tol + 2.f)) {
								const bool gauche = souris.x < p.x;
								if ((n->sockets[k].dir == graph::NkSocketDir::Input) != gauche) {
									continue;
								}
								c.noeud = n->id;
								c.prise = static_cast<int32>(k);
								return c;
							}
							continue;
						}
						// La zone d'une prise : sa languette, dehors, et un peu de la rangee.
						const bool entree = n->sockets[k].dir == graph::NkSocketDir::Input;
						const NkRect zp{entree ? p.x - tol : p.x - tol, p.y - Max(tol, s.priseHauteur * 0.5f * z + 2.f), tol * 2.f,
										Max(tol, s.priseHauteur * 0.5f * z + 2.f) * 2.f};
						if (Dans(zp, souris)) {
							c.noeud = n->id;
							c.prise = static_cast<int32>(k);
							return c;
						}
						if (c.champ < 0 && z >= s.palierValeurs) {
							const NkString v = ValeurLibre(g, *n, k, d);
							const NkGenreChamp gc = GenreChamp(g, *n, n->sockets[k], d);
							if (!v.Empty() && gc != NkGenreChamp::NK_AUCUN) {
								const NkRect rc = RectChamp(e, r, *n, k, s, gc);
								if (rc.w > 0.f && Dans(rc, souris)) {
									c.champNoeud = n->id;
									c.champ = static_cast<int32>(k);
								}
							}
						}
					}
					if (Dans(r, souris)) {
						if (geo != nullptr && geo->blocH > 0.f && c.champ < 0) {
							const NkRect rb{r.x, r.y + geo->blocY * z, r.w, geo->blocH * z};
							if (Dans(rb, souris)) {
								c.bloc = n->id;
								c.noeud = n->id;
								return c;
							}
						}
						c.noeud = n->id;
						return c;
					}
					if (c.champ >= 0) {
						return c;
					}
				}
				for (uint32 i = g.RawNodeCount(); i-- > 0u;) {
					const graph::NkNode *n = g.RawNodeAt(i);
					const NkGeomNoeud *geo = n != nullptr && n->alive ? Geo(e, n->id) : nullptr;
					if (geo == nullptr || geo->forme != NkFormeNoeud::NK_COMMENTAIRE) {
						continue;
					}
					const NkRect r = NkCanevasRectNoeud(e, zone, *n, s);
					const NkRect poignee{r.x + r.w - 14.f, r.y + r.h - 14.f, 14.f, 14.f};
					if (Dans(poignee, souris)) {
						c.commentaire = n->id;
						c.poignee = true;
						return c;
					}
					if (Dans(NkRect{r.x, r.y, r.w, s.hauteurEnteteCommentaire * z}, souris)) {
						c.commentaire = n->id;
						c.enteteCommentaire = true;
						return c;
					}
				}
				return c;
			}

			/// Les noeuds ENTIEREMENT dans le cadre `cm` (ils le suivent).
			void Contenus(const NkEtatCanevas &e, const graph::NkNodeGraph &g, const graph::NkNode &cm, NkVector<graph::NkNodeId> &sortie) {
				const NkGeomNoeud *gc = Geo(e, cm.id);
				if (gc == nullptr) {
					return;
				}
				for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
					const graph::NkNode *n = g.RawNodeAt(i);
					const NkGeomNoeud *gn = n != nullptr && n->alive ? Geo(e, n->id) : nullptr;
					if (gn == nullptr || n->id == cm.id || gn->forme == NkFormeNoeud::NK_COMMENTAIRE) {
						continue;
					}
					if (n->x >= cm.x && n->y >= cm.y && n->x + gn->w <= cm.x + gc->w && n->y + gn->h <= cm.y + gc->h) {
						sortie.PushBack(n->id);
					}
				}
			}

			void Groupe(NkEtatCanevas &e, const graph::NkNodeGraph &g, const NkVector<graph::NkNodeId> &ids) {
				e.gesteGroupe = ids;
				e.gesteOrigines.Clear();
				for (uint32 i = 0; i < ids.Size(); ++i) {
					const graph::NkNode *n = g.Find(ids[i]);
					e.gesteOrigines.PushBack(n != nullptr ? n->x : 0.f);
					e.gesteOrigines.PushBack(n != nullptr ? n->y : 0.f);
				}
			}

			/// Les icones de la barre d'outils, TRACEES (le kit n'a pas d'atlas).
			void Icone(nkgui::NkGuiDrawList &dl, NkOutilCanevas o, const NkRect &r, const NkColor &c) {
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, u = r.w * 0.16f;
				switch (o) {
					case NkOutilCanevas::NK_SELECTION: {
						const NkVec2 pts[] = {{cx - u * 1.2f, cy - u * 1.8f}, {cx + u * 1.4f, cy + u * 0.4f}, {cx + u * 0.1f, cy + u * 0.6f},
											  {cx - u * 0.4f, cy + u * 1.9f}};
						dl.AddTriangleFilled(pts[0], pts[1], pts[2], c);
						dl.AddTriangleFilled(pts[0], pts[2], pts[3], c);
						break;
					}
					case NkOutilCanevas::NK_MAIN: {
						dl.AddLine(NkVec2{cx - u * 1.8f, cy}, NkVec2{cx + u * 1.8f, cy}, c, 1.6f);
						dl.AddLine(NkVec2{cx, cy - u * 1.8f}, NkVec2{cx, cy + u * 1.8f}, c, 1.6f);
						const float32 t = u * 0.6f;
						TriangleLisse(dl, NkVec2{cx + u * 2.1f, cy}, NkVec2{cx + u * 1.4f, cy - t}, NkVec2{cx + u * 1.4f, cy + t}, c);
						TriangleLisse(dl, NkVec2{cx - u * 2.1f, cy}, NkVec2{cx - u * 1.4f, cy + t}, NkVec2{cx - u * 1.4f, cy - t}, c);
						TriangleLisse(dl, NkVec2{cx, cy - u * 2.1f}, NkVec2{cx + t, cy - u * 1.4f}, NkVec2{cx - t, cy - u * 1.4f}, c);
						TriangleLisse(dl, NkVec2{cx, cy + u * 2.1f}, NkVec2{cx - t, cy + u * 1.4f}, NkVec2{cx + t, cy + u * 1.4f}, c);
						break;
					}
					case NkOutilCanevas::NK_COMMENTAIRE: {
						// Le CADRE : double filet (plein dehors, pointille dedans).
						const NkRect b{cx - u * 2.f, cy - u * 1.6f, u * 4.f, u * 3.2f};
						dl.AddRect(b, c, 1.5f, 1.f);
						RectPointille(dl, NkRect{b.x + u * 0.7f, b.y + u * 0.9f, b.w - u * 1.4f, b.h - u * 1.6f}, c, 1.f, 2.f);
						dl.AddRectFilled(NkRect{b.x, b.y, b.w, u * 0.6f}, c);
						break;
					}
					case NkOutilCanevas::NK_RELAIS:
						dl.AddLine(NkVec2{cx - u * 2.f, cy + u}, NkVec2{cx - u * 0.5f, cy}, c, 1.6f);
						dl.AddLine(NkVec2{cx + u * 0.5f, cy}, NkVec2{cx + u * 2.f, cy - u}, c, 1.6f);
						dl.AddRectFilled(NkRect{cx - u * 0.7f, cy - u * 0.7f, u * 1.4f, u * 1.4f}, c, 1.f);
						break;
					default: {
						const float32 a = u * 1.8f, l = u * 1.1f;
						dl.AddLine(NkVec2{cx - a, cy - a}, NkVec2{cx - a + l, cy - a}, c, 1.6f);
						dl.AddLine(NkVec2{cx - a, cy - a}, NkVec2{cx - a, cy - a + l}, c, 1.6f);
						dl.AddLine(NkVec2{cx + a, cy + a}, NkVec2{cx + a - l, cy + a}, c, 1.6f);
						dl.AddLine(NkVec2{cx + a, cy + a}, NkVec2{cx + a, cy + a - l}, c, 1.6f);
						dl.AddLine(NkVec2{cx + a, cy - a}, NkVec2{cx + a - l, cy - a}, c, 1.6f);
						dl.AddLine(NkVec2{cx + a, cy - a}, NkVec2{cx + a, cy - a + l}, c, 1.6f);
						dl.AddLine(NkVec2{cx - a, cy + a}, NkVec2{cx - a + l, cy + a}, c, 1.6f);
						dl.AddLine(NkVec2{cx - a, cy + a}, NkVec2{cx - a, cy + a - l}, c, 1.6f);
						break;
					}
				}
			}

			/// Relie la prise (n, k) au PREMIER noeud qui l'accepte de `vers`.
			bool RelierAuNoeud(graph::NkNodeGraph &g, graph::NkNodeId n, int32 k, graph::NkNodeId vers) {
				const graph::NkNode *a = g.Find(n);
				const graph::NkNode *b = g.Find(vers);
				if (a == nullptr || b == nullptr || n == vers) {
					return false;
				}
				const graph::NkSocket sa = a->sockets[static_cast<uint32>(k)];
				const bool sort = sa.dir == graph::NkSocketDir::Output;
				for (uint32 j = 0; j < b->sockets.Size(); ++j) {
					const graph::NkSocket &sb = b->sockets[j];
					if (sb.dir == sa.dir || sb.family != sa.family) {
						continue;
					}
					if (sort && PriseLiee(g, b->id, static_cast<int32>(j)) && sb.family == graph::NkSocketFamily::Data) {
						continue; // une entree deja branchee n'est pas prise en priorite
					}
					const NkString nb = sb.name;
					const graph::NkLinkError err = sort ? g.Connect(n, sa.name.CStr(), vers, nb.CStr()) : g.Connect(vers, nb.CStr(), n, sa.name.CStr());
					if (err == graph::NkLinkError::Ok) {
						return true;
					}
				}
				return false;
			}

			/// La prise `b` accepte-t-elle un fil depuis `a` ? (pendant le tirage :
			/// les incompatibles s'eteignent, charte planche 04).
			bool Compatible(const graph::NkNodeGraph &g, const graph::NkNode &na, const graph::NkSocket &a, const graph::NkNode &nb,
							const graph::NkSocket &b) {
				if (na.id == nb.id || a.dir == b.dir || a.family != b.family) {
					return false;
				}
				const graph::NkSocket &sortie = a.dir == graph::NkSocketDir::Output ? a : b;
				const graph::NkSocket &entree = a.dir == graph::NkSocketDir::Output ? b : a;
				return a.family == graph::NkSocketFamily::Exec || g.Accepts(entree.type, sortie.type);
			}
		} // namespace

		void NkCanevasNoeuds(nkgui::NkGuiDrawList &dl, nkgui::NkGuiInput &in, nkgui::NkGuiFont *police, const NkRect &zone,
							 graph::NkNodeGraph &g, NkEtatCanevas &e, const NkStyleCanevas &s, const NkDomaineCanevas &d) {
			e.modifie = false;
			e.demande = NkDemandeCanevas::NK_AUCUNE;
			e.menuDemande = false;
			e.clavierPris = false;
			e.ageRefus += in.dt > 0.f ? in.dt : 1.f / 60.f;
			// Le choix ne garde que des noeuds vivants ; `selection` en fait partie.
			for (uint32 i = 0; i < e.choix.Size();) {
				if (g.Find(e.choix[i]) == nullptr) {
					e.choix.Erase(e.choix.Begin() + i);
				} else {
					++i;
				}
			}
			if (e.selection != graph::NK_NODE_INVALID && g.Find(e.selection) == nullptr) {
				e.selection = graph::NK_NODE_INVALID;
			}
			if (e.selection != graph::NK_NODE_INVALID && !e.EstChoisi(e.selection)) {
				e.choix.PushBack(e.selection);
			}
			// La trace : un domaine qui en a une la dit pour au moins un noeud.
			bool trace = false;
			if (d.Compte != nullptr) {
				for (uint32 i = 0; i < g.RawNodeCount() && !trace; ++i) {
					const graph::NkNode *n = g.RawNodeAt(i);
					trace = n != nullptr && n->alive && d.Compte(d.donnees, g, *n) >= 0;
				}
			}
			Mesurer(e, g, police, s, d, trace);
			const NkVec2 souris = in.mousePos;
			const bool dedans = Dans(zone, souris);

			// ── 1. LA SAISIE d'une valeur, si elle est ouverte ─────────────────
			auto validerSaisie = [&]() {
				if (e.saisieNoeud != graph::NK_NODE_INVALID && d.PoserDefaut != nullptr) {
					const graph::NkNode *n = g.Find(e.saisieNoeud);
					if (n != nullptr && e.saisiePrise >= 0 && static_cast<uint32>(e.saisiePrise) < n->sockets.Size()) {
						const NkString nom = n->sockets[static_cast<uint32>(e.saisiePrise)].name;
						if (d.PoserDefaut(d.donnees, g, e.saisieNoeud, nom.CStr(), e.saisie)) {
							e.modifie = true;
						} else {
							e.refus = NkString::Format("valeur refusée : « %s »", e.saisie);
							e.ageRefus = 0.f;
						}
					}
				}
				e.saisieNoeud = graph::NK_NODE_INVALID;
				e.saisiePrise = -1;
			};
			if (e.saisieNoeud != graph::NK_NODE_INVALID) {
				e.clavierPris = true;
				for (int32 i = 0; i < in.charCount; ++i) {
					const uint32 cp = in.chars[i];
					usize l = std::strlen(e.saisie);
					if (cp < 32u || l + 4u >= sizeof(e.saisie)) {
						continue;
					}
					if (cp < 0x80u) {
						e.saisie[l++] = static_cast<char>(cp);
					} else if (cp < 0x800u) {
						e.saisie[l++] = static_cast<char>(0xC0u | (cp >> 6));
						e.saisie[l++] = static_cast<char>(0x80u | (cp & 0x3Fu));
					} else {
						e.saisie[l++] = static_cast<char>(0xE0u | (cp >> 12));
						e.saisie[l++] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
						e.saisie[l++] = static_cast<char>(0x80u | (cp & 0x3Fu));
					}
					e.saisie[l] = '\0';
				}
				in.charCount = 0;
				if (in.KeyPressedRepeat(nkgui::NkGuiKey::Backspace)) {
					usize l = std::strlen(e.saisie);
					while (l > 0u && (static_cast<uint8>(e.saisie[l - 1u]) & 0xC0u) == 0x80u) {
						--l;
					}
					if (l > 0u) {
						--l;
					}
					e.saisie[l] = '\0';
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Enter)) {
					validerSaisie();
				} else if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
					e.saisieNoeud = graph::NK_NODE_INVALID;
					e.saisiePrise = -1;
				}
				for (int32 k = 0; k < nkgui::NkGuiInput::KeyCount; ++k) {
					in.keyInit[k] = false; // toutes les touches sont a la saisie
				}
			}

			// ── 2. LA BARRE D'OUTILS et la PORTEE (au-dessus de la toile) ─────
			bool surUi = false;
			if (e.barreOutils) {
				const float32 t = s.outilTaille;
				const uint32 nb = static_cast<uint32>(NkOutilCanevas::NK_NOMBRE);
				const NkRect panneau{zone.x + 12.f, zone.y + zone.h * 0.5f - (t * static_cast<float32>(nb) + 12.f) * 0.5f, t + 8.f,
									 t * static_cast<float32>(nb) + 12.f};
				for (uint32 i = 0; i < nb; ++i) {
					e.rectOutils[i] = NkRect{panneau.x + 4.f, panneau.y + 6.f + static_cast<float32>(i) * t, t, t - 4.f};
				}
				if (Dans(panneau, souris)) {
					surUi = true;
					if (in.mouseClicked[0] && e.geste == 0) {
						for (uint32 i = 0; i < nb; ++i) {
							if (Dans(e.rectOutils[i], souris)) {
								const NkOutilCanevas o = static_cast<NkOutilCanevas>(i);
								if (o == NkOutilCanevas::NK_CADRER) {
									NkCanevasCadrer(e, zone, g, s);
								} else {
									e.outil = o;
								}
							}
						}
						in.mouseClicked[0] = false;
					}
				}
			}
			if (!e.portee.Empty()) {
				const float32 lp = Largeur(police, e.portee.CStr(), 1.f) + Largeur(police, "Portée : ", 1.f) + 34.f;
				e.rectPortee = NkRect{zone.x + 12.f, zone.y + 10.f, lp, 26.f};
				if (Dans(e.rectPortee, souris)) {
					surUi = true;
					if (in.mouseClicked[0] && e.geste == 0) {
						e.demande = NkDemandeCanevas::NK_PORTEE;
						e.menuEcranX = e.rectPortee.x;
						e.menuEcranY = e.rectPortee.y + e.rectPortee.h + 2.f;
						in.mouseClicked[0] = false;
					}
				}
			}

			// ── 3. LES GESTES ───────────────────────────────────────────────────
			const Cible cible = (dedans && !surUi) ? Viser(e, zone, g, souris, s, d) : Cible();
			// Le fil sous la souris (rien d'autre dessous).
			e.filSurvole = 0;
			if (dedans && !surUi && cible.noeud == graph::NK_NODE_INVALID && cible.commentaire == graph::NK_NODE_INVALID && e.geste == 0) {
				float32 mieux = Carre(6.f);
				for (uint32 i = 0; i < g.LinkCount(); ++i) {
					const graph::NkLink *l = g.LinkAt(i);
					const graph::NkNode *a = l != nullptr && l->alive ? g.Find(l->fromNode) : nullptr;
					const graph::NkNode *b = l != nullptr && l->alive ? g.Find(l->toNode) : nullptr;
					NkVec2 pa, pb;
					if (a == nullptr || b == nullptr || !NkCanevasPrise(e, zone, *a, l->fromSocket, s, pa) ||
						!NkCanevasPrise(e, zone, *b, l->toSocket, s, pb)) {
						continue;
					}
					const float32 dd = DistanceAuFil(pa, pb, souris);
					if (dd < mieux) {
						mieux = dd;
						e.filSurvole = l->id;
					}
				}
			}
			auto insererRelais = [&](graph::NkLinkId lien) {
				const graph::NkLink *l = g.TrouveLien(lien);
				if (l == nullptr || d.CreerRelais == nullptr) {
					return;
				}
				const graph::NkNode *a = g.Find(l->fromNode);
				const graph::NkNode *b = g.Find(l->toNode);
				if (a == nullptr || b == nullptr) {
					return;
				}
				const graph::NkSocket sa = a->sockets[static_cast<uint32>(l->fromSocket)];
				const NkString nb = b->sockets[static_cast<uint32>(l->toSocket)].name;
				const graph::NkNodeId de = a->id, vers = b->id;
				const NkVec2 p = NkCanevasDepuisEcran(e, zone, souris);
				const graph::NkNodeId r = d.CreerRelais(d.donnees, g, sa.family == graph::NkSocketFamily::Exec ? "exec" : NomType(g, sa),
														p.x - s.rayonRelais, p.y - s.rayonRelais);
				if (r == graph::NK_NODE_INVALID) {
					return;
				}
				g.Disconnect(lien);
				g.Connect(de, sa.name.CStr(), r, "in");
				g.Connect(r, "out", vers, nb.CStr());
				e.Choisir(r);
				e.modifie = true;
			};

			// Sur un BLOC de domaine, seul le clic GAUCHE lui revient : la molette zoome,
			// le clic droit ouvre le menu du noeud (comme partout ailleurs sur le noeud).
			if (dedans && !surUi && e.geste == 0 && (cible.bloc == graph::NK_NODE_INVALID || !in.mouseClicked[0])) {
				// La molette : zoom autour du curseur.
				if (in.wheel != 0.f) {
					const NkVec2 avant = NkCanevasDepuisEcran(e, zone, souris);
					float32 nz = e.zoom * (in.wheel > 0.f ? 1.12f : 1.f / 1.12f);
					nz = nz < 0.2f ? 0.2f : (nz > 2.f ? 2.f : nz);
					e.zoom = nz;
					e.vueX = avant.x - (souris.x - zone.x) / nz;
					e.vueY = avant.y - (souris.y - zone.y) / nz;
					in.wheel = 0.f;
				}
				if (in.mouseClicked[0]) {
					if (e.saisieNoeud != graph::NK_NODE_INVALID) {
						validerSaisie();
					}
					e.appuiX = souris.x;
					e.appuiY = souris.y;
					e.gesteBouge = false;
					if (cible.prise >= 0) {
						if (in.altDown) {
							// Alt + clic : couper les fils de la prise (UE5).
							for (uint32 i = 0; i < g.LinkCount(); ++i) {
								const graph::NkLink *l = g.LinkAt(i);
								if (l != nullptr && l->alive &&
									((l->fromNode == cible.noeud && l->fromSocket == cible.prise) || (l->toNode == cible.noeud && l->toSocket == cible.prise))) {
									g.Disconnect(l->id);
									e.modifie = true;
								}
							}
						} else {
							e.geste = 2;
							e.gesteNoeud = cible.noeud;
							e.gestePrise = cible.prise;
						}
					} else if (cible.champ >= 0) {
						const graph::NkNode *n = g.Find(cible.champNoeud);
						const graph::NkSocket &so = n->sockets[static_cast<uint32>(cible.champ)];
						const NkString t = d.TexteDefaut(d.donnees, g, *n, so);
						e.Choisir(cible.champNoeud);
						if (GenreChamp(g, *n, so, d) == NkGenreChamp::NK_CASE) {
							// Une case a cocher : un clic la bascule (jamais un champ).
							const bool vrai = t == "vrai" || t == "1" || t == "true";
							const NkString nom = so.name;
							if (d.PoserDefaut != nullptr && d.PoserDefaut(d.donnees, g, cible.champNoeud, nom.CStr(), vrai ? "faux" : "vrai")) {
								e.modifie = true;
							}
						} else {
							e.saisieNoeud = cible.champNoeud;
							e.saisiePrise = cible.champ;
							std::snprintf(e.saisie, sizeof(e.saisie), "%s", t.CStr());
						}
					} else if (cible.noeud != graph::NK_NODE_INVALID) {
						if (in.mouseDoubleClicked[0]) {
							e.demande = NkDemandeCanevas::NK_DOUBLE_CLIC;
							e.menuNoeud = cible.noeud;
						}
						if (in.ctrlDown) {
							if (e.EstChoisi(cible.noeud)) {
								for (uint32 i = 0; i < e.choix.Size(); ++i) {
									if (e.choix[i] == cible.noeud) {
										e.choix.Erase(e.choix.Begin() + i);
										break;
									}
								}
								e.selection = e.choix.Empty() ? graph::NK_NODE_INVALID : e.choix.Back();
							} else {
								e.choix.PushBack(cible.noeud);
								e.selection = cible.noeud;
							}
						} else if (in.shiftDown) {
							if (!e.EstChoisi(cible.noeud)) {
								e.choix.PushBack(cible.noeud);
							}
							e.selection = cible.noeud;
						} else {
							if (!e.EstChoisi(cible.noeud)) {
								e.Choisir(cible.noeud);
							}
							e.selection = cible.noeud;
						}
						if (!in.ctrlDown) {
							e.geste = 1;
							e.gesteNoeud = cible.noeud;
							Groupe(e, g, e.choix);
						}
					} else if (cible.commentaire != graph::NK_NODE_INVALID) {
						const graph::NkNode *cm = g.Find(cible.commentaire);
						e.Choisir(cible.commentaire);
						if (cible.poignee && cm != nullptr) {
							e.geste = 6;
							e.gesteNoeud = cible.commentaire;
							const NkGeomNoeud *gc = Geo(e, cm->id);
							e.gesteDx = gc != nullptr ? gc->w : 320.f;
							e.gesteDy = gc != nullptr ? gc->h : 180.f;
						} else if (cm != nullptr) {
							if (in.mouseDoubleClicked[0]) {
								e.demande = NkDemandeCanevas::NK_DOUBLE_CLIC;
								e.menuNoeud = cible.commentaire;
							}
							// Tirer le BANDEAU deplace le cadre ET ses membres (planche 05).
							NkVector<graph::NkNodeId> ids;
							ids.PushBack(cm->id);
							Contenus(e, g, *cm, ids);
							e.geste = 5;
							e.gesteNoeud = cible.commentaire;
							Groupe(e, g, ids);
						}
					} else if (e.filSurvole != 0 && (in.mouseDoubleClicked[0] || e.outil == NkOutilCanevas::NK_RELAIS)) {
						insererRelais(e.filSurvole);
					} else if (e.outil == NkOutilCanevas::NK_MAIN) {
						e.geste = 3;
						e.bouton = 0;
						e.gesteDx = e.vueX;
						e.gesteDy = e.vueY;
					} else if (e.outil == NkOutilCanevas::NK_COMMENTAIRE) {
						e.geste = 7;
					} else {
						// Le vide : le LASSO (Ctrl / Maj : ajouter au choix).
						if (!in.ctrlDown && !in.shiftDown) {
							e.Choisir(graph::NK_NODE_INVALID);
						}
						e.geste = 4;
						Groupe(e, g, e.choix);
					}
				} else if (in.mouseClicked[1] || in.mouseClicked[2]) {
					const int32 b = in.mouseClicked[1] ? 1 : 2;
					if (b == 1 && cible.prise >= 0) {
						// Couper les fils de cette prise.
						for (uint32 i = 0; i < g.LinkCount(); ++i) {
							const graph::NkLink *l = g.LinkAt(i);
							if (l != nullptr && l->alive &&
								((l->fromNode == cible.noeud && l->fromSocket == cible.prise) || (l->toNode == cible.noeud && l->toSocket == cible.prise))) {
								g.Disconnect(l->id);
								e.modifie = true;
							}
						}
					} else if (b == 1 && (cible.noeud != graph::NK_NODE_INVALID || cible.commentaire != graph::NK_NODE_INVALID)) {
						const graph::NkNodeId n = cible.noeud != graph::NK_NODE_INVALID ? cible.noeud : cible.commentaire;
						if (!e.EstChoisi(n)) {
							e.Choisir(n);
						}
						e.selection = n;
						e.demande = NkDemandeCanevas::NK_MENU_NOEUD;
						e.menuNoeud = n;
						e.menuEcranX = souris.x;
						e.menuEcranY = souris.y;
						const NkVec2 p = NkCanevasDepuisEcran(e, zone, souris);
						e.menuX = p.x;
						e.menuY = p.y;
					} else {
						e.geste = 3;
						e.bouton = b;
						e.gesteDx = e.vueX;
						e.gesteDy = e.vueY;
						e.appuiX = souris.x;
						e.appuiY = souris.y;
					}
				}
			} else if (dedans && cible.bloc != graph::NK_NODE_INVALID && in.mouseClicked[0] && e.geste == 0) {
				// Le BLOC est au domaine ; le noeud devient le choix.
				if (!e.EstChoisi(cible.bloc)) {
					e.Choisir(cible.bloc);
				}
				e.selection = cible.bloc;
			}
			const bool bouge = Carre(souris.x - e.appuiX) + Carre(souris.y - e.appuiY) >= 16.f;
			e.gesteBouge = e.gesteBouge || (e.geste != 0 && bouge);
			if (e.geste == 1 || e.geste == 5) {
				// Deplacer le groupe (calage sur une grille de 8).
				if (e.gesteBouge) {
					const float32 dx = (souris.x - e.appuiX) / e.zoom, dy = (souris.y - e.appuiY) / e.zoom;
					const float32 cal = 8.f;
					for (uint32 i = 0; i < e.gesteGroupe.Size(); ++i) {
						if (graph::NkNode *n = g.Find(e.gesteGroupe[i])) {
							const float32 x = e.gesteOrigines[2u * i] + dx, y = e.gesteOrigines[2u * i + 1u] + dy;
							n->x = static_cast<float32>(static_cast<int32>(x / cal + (x >= 0.f ? 0.5f : -0.5f))) * cal;
							n->y = static_cast<float32>(static_cast<int32>(y / cal + (y >= 0.f ? 0.5f : -0.5f))) * cal;
						}
					}
				}
				if (!in.mouseDown[0]) {
					if (e.gesteBouge) {
						e.modifie = true; // la position est dans le fichier
					} else if (e.geste == 1 && !in.shiftDown && !in.ctrlDown) {
						e.Choisir(e.gesteNoeud); // un clic sans bouger : lui seul
					}
					e.geste = 0;
				}
			} else if (e.geste == 3) {
				e.vueX = e.gesteDx - (souris.x - e.appuiX) / e.zoom;
				e.vueY = e.gesteDy - (souris.y - e.appuiY) / e.zoom;
				if (!in.mouseDown[e.bouton]) {
					// Un clic droit SANS deplacement dans le vide : le menu de la toile.
					if (e.bouton == 1 && !e.gesteBouge) {
						const NkVec2 p = NkCanevasDepuisEcran(e, zone, souris);
						e.demande = NkDemandeCanevas::NK_MENU_TOILE;
						e.menuDemande = true;
						e.menuX = p.x;
						e.menuY = p.y;
						e.menuEcranX = souris.x;
						e.menuEcranY = souris.y;
						e.menuNoeud = graph::NK_NODE_INVALID;
						e.menuPrise = -1;
					}
					e.geste = 0;
				}
			} else if (e.geste == 4) {
				// Le lasso : ce qu'il touche est choisi (en plus du choix de depart).
				const NkVec2 a = NkCanevasDepuisEcran(e, zone, NkVec2{e.appuiX, e.appuiY});
				const NkVec2 b = NkCanevasDepuisEcran(e, zone, souris);
				const float32 x0 = Min(a.x, b.x), y0 = Min(a.y, b.y), x1 = Max(a.x, b.x), y1 = Max(a.y, b.y);
				e.choix = e.gesteGroupe;
				if (e.gesteBouge) {
					for (uint32 i = 0; i < e.geom.Size(); ++i) {
						const NkGeomNoeud &gn = e.geom[i];
						const graph::NkNode *n = g.Find(gn.id);
						if (n == nullptr || gn.forme == NkFormeNoeud::NK_COMMENTAIRE || e.EstChoisi(gn.id)) {
							continue;
						}
						if (n->x < x1 && n->x + gn.w > x0 && n->y < y1 && n->y + gn.h > y0) {
							e.choix.PushBack(gn.id);
						}
					}
				}
				e.selection = e.choix.Empty() ? graph::NK_NODE_INVALID : e.choix.Back();
				if (!in.mouseDown[0]) {
					e.geste = 0;
				}
			} else if (e.geste == 6) {
				if (d.PoserTailleCommentaire != nullptr) {
					const float32 w = Max(120.f, e.gesteDx + (souris.x - e.appuiX) / e.zoom);
					const float32 h = Max(60.f, e.gesteDy + (souris.y - e.appuiY) / e.zoom);
					d.PoserTailleCommentaire(d.donnees, g, e.gesteNoeud, w, h);
				}
				if (!in.mouseDown[0]) {
					e.geste = 0;
					e.modifie = true;
				}
			} else if (e.geste == 7) {
				if (!in.mouseDown[0]) {
					const NkVec2 a = NkCanevasDepuisEcran(e, zone, NkVec2{e.appuiX, e.appuiY});
					const NkVec2 b = NkCanevasDepuisEcran(e, zone, souris);
					if (d.CreerCommentaire != nullptr) {
						const float32 w = Max(160.f, Absf(b.x - a.x)), h = Max(90.f, Absf(b.y - a.y));
						const graph::NkNodeId c = d.CreerCommentaire(d.donnees, g, Min(a.x, b.x), Min(a.y, b.y), w, h);
						if (c != graph::NK_NODE_INVALID) {
							e.Choisir(c);
							e.modifie = true;
						}
					}
					e.outil = NkOutilCanevas::NK_SELECTION;
					e.geste = 0;
				}
			} else if (e.geste == 2 && !in.mouseDown[0]) {
				// Le fil est lache : sur une prise, on tente ; le refus se NOMME.
				const Cible sous = Viser(e, zone, g, souris, s, d);
				if (sous.prise >= 0 && !(sous.noeud == e.gesteNoeud && sous.prise == e.gestePrise)) {
					const graph::NkNode *a = g.Find(e.gesteNoeud);
					const graph::NkNode *b = g.Find(sous.noeud);
					if (a != nullptr && b != nullptr) {
						const graph::NkSocket &sa = a->sockets[static_cast<uint32>(e.gestePrise)];
						const graph::NkSocket &sb = b->sockets[static_cast<uint32>(sous.prise)];
						const bool aSort = sa.dir == graph::NkSocketDir::Output;
						const NkString nsa = sa.name, nsb = sb.name;
						const graph::NkNodeId de = aSort ? a->id : b->id;
						const graph::NkNodeId vers = aSort ? b->id : a->id;
						const graph::NkLinkError err = g.Connect(de, aSort ? nsa.CStr() : nsb.CStr(), vers, aSort ? nsb.CStr() : nsa.CStr());
						if (err == graph::NkLinkError::Ok) {
							e.modifie = true;
						} else {
							e.refus = NkString::Format("fil refusé : %s", graph::NkLinkErrorName(err));
							e.ageRefus = 0.f;
						}
					}
				} else if (sous.noeud != graph::NK_NODE_INVALID && sous.noeud != e.gesteNoeud) {
					// Lache sur le CORPS d'un noeud : la premiere prise qui l'accepte.
					if (RelierAuNoeud(g, e.gesteNoeud, e.gestePrise, sous.noeud)) {
						e.modifie = true;
					} else {
						e.refus = NkString("fil refusé : aucune prise compatible sur ce nœud");
						e.ageRefus = 0.f;
					}
				} else if (sous.noeud == graph::NK_NODE_INVALID && e.gesteBouge && dedans) {
					// Lache dans le VIDE : le menu SENSIBLE AU CONTEXTE (UE5 ; charte,
					// planche 02 : « la recherche s'ouvre, FILTREE »).
					const NkVec2 p = NkCanevasDepuisEcran(e, zone, souris);
					e.demande = NkDemandeCanevas::NK_MENU_PRISE;
					e.menuNoeud = e.gesteNoeud;
					e.menuPrise = e.gestePrise;
					e.menuX = p.x;
					e.menuY = p.y;
					e.menuEcranX = souris.x;
					e.menuEcranY = souris.y;
				}
				e.geste = 0;
			}

			// ── 4. LE CLAVIER (la souris sur la toile, aucune saisie) ───────────
			if (dedans && e.saisieNoeud == graph::NK_NODE_INVALID && e.geste == 0 && !e.clavierAilleurs) {
				auto prendre = [&](nkgui::NkGuiKey k) {
					in.keyInit[static_cast<int32>(k)] = false;
					e.clavierPris = true;
				};
				const NkVec2 p = NkCanevasDepuisEcran(e, zone, souris);
				if (in.KeyPressed(nkgui::NkGuiKey::Delete)) {
					NkCanevasSupprimerChoix(g, e, d);
					prendre(nkgui::NkGuiKey::Delete);
				} else if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::C)) {
					e.pressePapiers = NkCanevasCopier(g, e, d);
					prendre(nkgui::NkGuiKey::C);
				} else if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::X)) {
					e.pressePapiers = NkCanevasCopier(g, e, d);
					NkCanevasSupprimerChoix(g, e, d);
					prendre(nkgui::NkGuiKey::X);
				} else if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::V)) {
					NkCanevasColler(g, e, e.pressePapiers.CStr(), p.x, p.y);
					prendre(nkgui::NkGuiKey::V);
				} else if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::D)) {
					float32 bx = 0.f, by = 0.f, bw = 0.f, bh = 0.f;
					const NkString t = NkCanevasCopier(g, e, d);
					if (NkCanevasBoiteChoix(g, e, s, bx, by, bw, bh)) {
						NkCanevasColler(g, e, t.CStr(), bx + 24.f + 32.f, by + 24.f + s.hauteurEnteteCommentaire + 32.f);
					}
					prendre(nkgui::NkGuiKey::D);
				} else if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::A)) {
					e.choix.Clear();
					for (uint32 i = 0; i < e.geom.Size(); ++i) {
						e.choix.PushBack(e.geom[i].id);
					}
					e.selection = e.choix.Empty() ? graph::NK_NODE_INVALID : e.choix.Back();
					prendre(nkgui::NkGuiKey::A);
				} else if (!in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::C) && d.CreerCommentaire != nullptr) {
					// C : un CADRE autour du choix (ou sous la souris).
					float32 bx = p.x, by = p.y, bw = 320.f, bh = 180.f;
					NkCanevasBoiteChoix(g, e, s, bx, by, bw, bh);
					const graph::NkNodeId c = d.CreerCommentaire(d.donnees, g, bx, by, bw, bh);
					if (c != graph::NK_NODE_INVALID) {
						e.Choisir(c);
						e.modifie = true;
					}
					prendre(nkgui::NkGuiKey::C);
				} else if (!in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::Q)) {
					NkCanevasRedresser(g, e, s);
					prendre(nkgui::NkGuiKey::Q);
				} else if (in.shiftDown && in.KeyPressed(nkgui::NkGuiKey::W)) {
					NkCanevasAligner(g, e, 2);
					prendre(nkgui::NkGuiKey::W);
				} else if (in.shiftDown && in.KeyPressed(nkgui::NkGuiKey::S)) {
					NkCanevasAligner(g, e, 3);
					prendre(nkgui::NkGuiKey::S);
				} else if (in.shiftDown && in.KeyPressed(nkgui::NkGuiKey::A)) {
					NkCanevasAligner(g, e, 0);
					prendre(nkgui::NkGuiKey::A);
				} else if (in.shiftDown && in.KeyPressed(nkgui::NkGuiKey::D)) {
					NkCanevasAligner(g, e, 1);
					prendre(nkgui::NkGuiKey::D);
				} else if (in.KeyPressed(nkgui::NkGuiKey::Home)) {
					NkCanevasCadrer(e, zone, g, s);
					prendre(nkgui::NkGuiKey::Home);
				} else if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
					e.Choisir(graph::NK_NODE_INVALID);
					e.outil = NkOutilCanevas::NK_SELECTION;
					prendre(nkgui::NkGuiKey::Escape);
				}
			}
			if (e.modifie) {
				Mesurer(e, g, police, s, d, trace); // le graphe a change : la geometrie suit
			}

			// ── 5. LE DESSIN ────────────────────────────────────────────────────
			const float32 z = e.zoom;
			const float32 lh = HauteurLigne(police) * z;
			const bool valeurs = z >= s.palierValeurs;
			const bool rectangles = z < s.palierRectangle;
			dl.PushClipRect(zone);
			dl.AddRectFilled(zone, s.fond);
			// La grille de POINTS (planche 07 : « des POINTS, jamais des lignes »).
			{
				const float32 pas = s.pasGrille * z;
				if (pas >= 7.f) {
					const float32 tp = s.taillePoint;
					const int32 kx0 = static_cast<int32>(e.vueX / s.pasGrille) - (e.vueX < 0.f ? 1 : 0);
					const int32 ky0 = static_cast<int32>(e.vueY / s.pasGrille) - (e.vueY < 0.f ? 1 : 0);
					for (int32 ky = ky0;; ++ky) {
						const float32 y = zone.y + (static_cast<float32>(ky) * s.pasGrille - e.vueY) * z;
						if (y > zone.y + zone.h) {
							break;
						}
						for (int32 kx = kx0;; ++kx) {
							const float32 x = zone.x + (static_cast<float32>(kx) * s.pasGrille - e.vueX) * z;
							if (x > zone.x + zone.w) {
								break;
							}
							dl.AddRectFilled(NkRect{x - tp * 0.5f, y - tp * 0.5f, tp, tp}, s.point);
						}
					}
				}
			}
			auto compte = [&](const graph::NkNode &n) -> int32 {
				return trace ? d.Compte(d.donnees, g, n) : -1;
			};
			// Les CADRES, derriere tout (planche 05 : option A, double filet).
			for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
				const graph::NkNode *n = g.RawNodeAt(i);
				const NkGeomNoeud *geo = n != nullptr && n->alive ? Geo(e, n->id) : nullptr;
				if (geo == nullptr || geo->forme != NkFormeNoeud::NK_COMMENTAIRE) {
					continue;
				}
				const NkRect r = NkCanevasRectNoeud(e, zone, *n, s);
				const float32 he = s.hauteurEnteteCommentaire * z;
				dl.AddRectFilled(r, NkAlphaNodal(s.cadre, s.cadreRemplissage), 6.f * z);
				dl.AddRect(r, e.EstChoisi(n->id) ? s.selection : s.cadre, 1.5f, 6.f * z);
				const float32 ri = s.cadreRetrait * z;
				RectPointille(dl, NkRect{r.x + ri, r.y + he + ri, r.w - 2.f * ri, r.h - he - 2.f * ri}, NkAlphaNodal(s.cadre, 0.8f), 1.f, 4.f);
				dl.AddRectFilled(NkRect{r.x, r.y, r.w, he}, s.cadre, 6.f * z);
				dl.AddRectFilled(NkRect{r.x, r.y + he * 0.5f, r.w, he * 0.5f}, s.cadre);
				if (!rectangles) {
					NkVector<graph::NkNodeId> membres;
					Contenus(e, g, *n, membres);
					char cpt[32];
					std::snprintf(cpt, sizeof(cpt), "%u nœud%s", static_cast<unsigned>(membres.Size()), membres.Size() > 1u ? "s" : "");
					const float32 wc = Largeur(police, cpt, z * 0.86f);
					Texte(dl, police, r.x + 8.f * z, r.y + (he - lh * 0.92f) * 0.5f, n->label.CStr(), s.cadreTexte, z * 0.92f, r.w - wc - 28.f * z);
					Texte(dl, police, r.x + r.w - wc - 8.f * z, r.y + (he - lh * 0.86f) * 0.5f, cpt, s.cadreTexte, z * 0.86f);
				}
				dl.AddLine(NkVec2{r.x + r.w - 11.f, r.y + r.h - 3.f}, NkVec2{r.x + r.w - 3.f, r.y + r.h - 11.f}, s.cadre, 1.f);
				dl.AddLine(NkVec2{r.x + r.w - 7.f, r.y + r.h - 3.f}, NkVec2{r.x + r.w - 3.f, r.y + r.h - 7.f}, s.cadre, 1.f);
			}
			// Les FILS, sous les noeuds : valeur 2 px couleur du type, execution
			// 3,5 px TOUJOURS orange. Sous la trace : la lueur des fils qui ont servi.
			for (int32 passe = 0; passe < 2; ++passe) {
				for (uint32 i = 0; i < g.LinkCount(); ++i) {
					const graph::NkLink *l = g.LinkAt(i);
					if (l == nullptr || !l->alive) {
						continue;
					}
					const graph::NkNode *a = g.Find(l->fromNode);
					const graph::NkNode *b = g.Find(l->toNode);
					NkVec2 pa, pb;
					if (a == nullptr || b == nullptr || !NkCanevasPrise(e, zone, *a, l->fromSocket, s, pa) ||
						!NkCanevasPrise(e, zone, *b, l->toSocket, s, pb)) {
						continue;
					}
					const graph::NkSocket &sa = a->sockets[static_cast<uint32>(l->fromSocket)];
					const bool exec = sa.family == graph::NkSocketFamily::Exec;
					// Le fil part du BORD de la prise : en dehors de la languette.
					const int32 ca = compte(*a), cb = compte(*b);
					const bool servi = trace && exec && ca > 0 && cb > 0;
					if ((passe == 1) != exec) {
						continue; // l'execution par-dessus la valeur
					}
					const float32 ep = (exec ? s.epaisseurExec : s.epaisseurDonnee) * (z < 0.6f ? 0.6f : z);
					NkColor c = CouleurPrise(g, sa, s, d);
					if (trace && (ca == 0 || cb == 0 || (exec && !servi))) {
						c = NkAlphaNodal(c, s.alphaEstompe);
					}
					if (servi) {
						const float32 large = s.lueurLarge * (z < 0.6f ? 0.6f : z);
						Fil(dl, pa, pb, NkAlphaNodal(s.lueur, 0.10f), large);
						Fil(dl, pa, pb, NkAlphaNodal(s.lueur, 0.20f), large * 0.6f);
					}
					if (l->id == e.filSurvole) {
						Fil(dl, pa, pb, NkAlphaNodal(c, 0.35f), ep + 5.f);
					}
					Fil(dl, pa, pb, c, ep);
				}
			}
			// Le fil que l'on tire : POINTILLE gris (planche 02).
			const graph::NkNode *tireNoeud = e.geste == 2 ? g.Find(e.gesteNoeud) : nullptr;
			const graph::NkSocket *tirePrise = tireNoeud != nullptr ? &tireNoeud->sockets[static_cast<uint32>(e.gestePrise)] : nullptr;
			// Les NOEUDS.
			for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
				const graph::NkNode *n = g.RawNodeAt(i);
				const NkGeomNoeud *geo = n != nullptr && n->alive ? Geo(e, n->id) : nullptr;
				if (geo == nullptr || geo->forme == NkFormeNoeud::NK_COMMENTAIRE) {
					continue;
				}
				const NkRect r = NkCanevasRectNoeud(e, zone, *n, s);
				if (r.x > zone.x + zone.w + 20.f || r.y > zone.y + zone.h + 20.f || r.x + r.w < zone.x - 20.f || r.y + r.h < zone.y - 20.f) {
					continue;
				}
				const int32 cpt = compte(*n);
				const float32 alpha = (trace && cpt == 0) ? 0.45f : 1.f;
				const bool erreur = n->id == e.enErreur || !geo->erreur.Empty();
				const bool choisi = e.EstChoisi(n->id);
				const bool survole = dedans && e.geste == 0 && Dans(r, souris);
				const NkNatureNoeud nature = d.Nature != nullptr ? d.Nature(d.donnees, *n) : NkNatureNoeud::NK_DEFAUT;
				const uint32 ni = static_cast<uint32>(nature) < static_cast<uint32>(NkNatureNoeud::NK_NOMBRE) ? static_cast<uint32>(nature) : 0u;
				const NkColor teinte = d.CouleurEntete != nullptr ? d.CouleurEntete(d.donnees, *n) : s.entete[ni];
				const bool execute = Execute(*n);
				const NkColor filet = execute ? s.filetExecute : s.filetCalcule;
				if (geo->forme == NkFormeNoeud::NK_RELAIS) {
					// Le relais NU : un petit carre de la couleur du type.
					NkColor col = n->sockets.Empty() ? s.exec : CouleurPrise(g, n->sockets[0], s, d);
					const NkRect q{r.x, r.y, r.w, r.h};
					dl.AddRectFilled(q, NkAlphaNodal(col, alpha), 2.f * z);
					if (choisi) {
						dl.AddRect(NkRect{q.x - 3.f, q.y - 3.f, q.w + 6.f, q.h + 6.f}, s.selection, s.epaisseurSelection, 3.f * z);
					}
					continue;
				}
				if (geo->forme == NkFormeNoeud::NK_COMPACT) {
					// La PUCE (le « relais nomme » de la reference) : un bloc-icone
					// de la couleur du type a gauche, le libelle, une sortie.
					NkColor tt = s.exec;
					const graph::NkSocket *so = nullptr;
					for (uint32 k = 0; k < n->sockets.Size(); ++k) {
						if (n->sockets[k].dir == graph::NkSocketDir::Output) {
							so = &n->sockets[k];
						}
					}
					if (so != nullptr) {
						tt = CouleurPrise(g, *so, s, d);
					}
					dl.AddRectFilled(NkRect{r.x + 2.f * z, r.y + 3.f * z, r.w, r.h}, NkAlphaNodal(s.ombre, alpha), r.h * 0.5f);
					dl.AddRectFilled(r, NkAlphaNodal(s.corps, alpha), r.h * 0.5f);
					const NkRect bloc{r.x, r.y, 28.f * z, r.h};
					dl.AddRectFilled(bloc, NkAlphaNodal(NkColor(static_cast<uint8>(tt.r / 3), static_cast<uint8>(tt.g / 3), static_cast<uint8>(tt.b / 3), 255), alpha),
									 r.h * 0.5f);
					dl.AddRectFilled(NkRect{bloc.x + bloc.w * 0.5f, bloc.y, bloc.w * 0.5f, bloc.h}, NkAlphaNodal(NkColor(static_cast<uint8>(tt.r / 3), static_cast<uint8>(tt.g / 3), static_cast<uint8>(tt.b / 3), 255), alpha));
					if (!rectangles && so != nullptr) {
						const char *gl = NkGlypheTypeNodal(NomType(g, *so));
						Texte(dl, police, bloc.x + (bloc.w - Largeur(police, gl, z * 0.72f)) * 0.5f, r.y + (r.h - lh * 0.72f) * 0.5f, gl, NkAlphaNodal(tt, alpha), z * 0.72f);
						Texte(dl, police, bloc.x + bloc.w + 8.f * z, r.y + (r.h - lh) * 0.5f, n->label.CStr(), NkAlphaNodal(s.texteEntete, alpha), z,
							  r.w - bloc.w - 18.f * z);
					}
					const NkColor bc = erreur ? s.erreur : (choisi ? s.selection : (survole ? s.survol : s.bord));
					dl.AddRect(r, NkAlphaNodal(bc, alpha), choisi || erreur || survole ? s.epaisseurSelection : 1.f, r.h * 0.5f);
					for (uint32 k = 0; k < n->sockets.Size(); ++k) {
						NkVec2 p;
						NkCanevasPrise(e, zone, *n, static_cast<int32>(k), s, p);
						const bool liee = PriseLiee(g, n->id, static_cast<int32>(k));
						PriseDonnee(dl, p.x, p.y, s.priseLargeur * z, s.priseHauteur * z, NkAlphaNodal(CouleurPrise(g, n->sockets[k], s, d), alpha), s.corps, liee,
									s.priseContour);
					}
					continue;
				}
				// ── Le noeud NORMAL ──
				const float32 he = geo->entete * z;
				const float32 rEn = s.rayonEntete * z;
				const bool evenement = nature == NkNatureNoeud::NK_EVENEMENT;
				// L'ombre, le corps (rayon 0), l'en-tete (rayon 5 ; 12 en haut a gauche
				// pour un evenement).
				dl.AddRectFilled(NkRect{r.x + 3.f * z, r.y + 4.f * z, r.w, r.h}, NkAlphaNodal(s.ombre, alpha), 0.f);
				dl.AddRectFilled(r, NkAlphaNodal(erreur ? s.corpsErreur : s.corps, alpha), 0.f);
				if (rectangles) {
					// Palier 25 % : un RECTANGLE de la categorie, et le filet. Rien d'autre.
					dl.AddRectFilled(r, NkAlphaNodal(teinte, alpha));
					dl.AddRectFilled(NkRect{r.x, r.y + r.h - Max(2.f, s.filet * z), r.w, Max(2.f, s.filet * z)}, NkAlphaNodal(filet, alpha));
					if (choisi || erreur) {
						dl.AddRect(r, erreur ? s.erreur : s.selection, s.epaisseurSelection);
					}
					continue;
				}
				const float32 rTete = evenement ? s.rayonEvenement * z : rEn;
				dl.AddRectFilled(NkRect{r.x, r.y, r.w, he}, NkAlphaNodal(teinte, alpha), rTete);
				// Les coins du BAS de l'en-tete sont carres (et le haut droit d'un
				// evenement garde son rayon de 5).
				dl.AddRectFilled(NkRect{r.x, r.y + he * 0.5f, r.w, he * 0.5f}, NkAlphaNodal(teinte, alpha));
				if (evenement) {
					dl.AddRectFilled(NkRect{r.x + r.w * 0.5f, r.y, r.w * 0.5f, he}, NkAlphaNodal(teinte, alpha), rEn);
					dl.AddRectFilled(NkRect{r.x + r.w * 0.5f, r.y + he * 0.5f, r.w * 0.5f, he * 0.5f}, NkAlphaNodal(teinte, alpha));
				}
				// Le FILET : la nature (execute / calcule).
				dl.AddRectFilled(NkRect{r.x, r.y + he, r.w, Max(1.5f, s.filet * z)}, NkAlphaNodal(filet, alpha));
				// Le titre : « ▼ », le titre, « ? » (ou « ! » en erreur) a droite.
				const NkColor tc = nature == NkNatureNoeud::NK_SORTIE ? s.texteEnteteSombre : s.texteEntete;
				const float32 ty = r.y + (s.hauteurEntete * z - lh) * 0.5f + 1.f * z;
				const float32 tx = r.x + 9.f * z;
				TriangleLisse(dl, NkVec2{tx, ty + lh * 0.38f}, NkVec2{tx + 7.f * z, ty + lh * 0.38f}, NkVec2{tx + 3.5f * z, ty + lh * 0.38f + 5.f * z},
									 NkAlphaNodal(nature == NkNatureNoeud::NK_SORTIE ? s.texteEnteteSombre : s.triangleTitre, alpha));
				float32 droite = 8.f * z;
				{
					const char *aide = erreur ? "!" : "?";
					const float32 wa = Largeur(police, aide, z);
					Texte(dl, police, r.x + r.w - droite - wa, ty, aide, NkAlphaNodal(erreur ? s.erreur : s.aideTitre, alpha), z);
					droite += wa + 6.f * z;
				}
				if (trace && cpt >= 0) {
					// (PROPOSE) le compteur de la trace : « x3 », a cote du « ? ».
					char b[16];
					std::snprintf(b, sizeof(b), "×%d", static_cast<int>(cpt));
					const NkColor cc = cpt > 0 ? s.compteur : s.compteurZero;
					const float32 bw = Largeur(police, b, z * 0.8f) + 10.f * z;
					const NkRect rb{r.x + r.w - droite - bw, ty + (lh - 15.f * z) * 0.5f, bw, 15.f * z};
					dl.AddRectFilled(rb, NkAlphaNodal(cc, 0.24f), 3.f * z);
					Texte(dl, police, rb.x + 5.f * z, rb.y + (rb.h - lh * 0.8f) * 0.5f, b, cc, z * 0.8f);
					droite += bw + 6.f * z;
				}
				Texte(dl, police, tx + 12.f * z, ty, n->label.Empty() ? n->type.CStr() : n->label.CStr(), NkAlphaNodal(tc, alpha), z,
					  r.w - (tx + 12.f * z - r.x) - droite);
				if (d.SousTitre != nullptr && geo->entete > s.hauteurEntete) {
					const NkString st = d.SousTitre(d.donnees, g, *n);
					Texte(dl, police, tx + 12.f * z, r.y + s.hauteurEntete * z - 3.f * z, st.CStr(), NkAlphaNodal(tc, alpha * 0.85f), z * 0.78f,
						  r.w - 30.f * z);
				}
				// Le contour : survol / selection / erreur (charte, planche 04).
				const NkColor bc = erreur ? s.erreur : (choisi ? s.selection : (survole ? s.survol : s.bord));
				dl.AddRect(r, NkAlphaNodal(bc, alpha), erreur || choisi || survole ? s.epaisseurSelection : 1.f, 0.f);
				// Les PRISES et leurs rangees.
				const int32 tete = ExecEnTete(*n);
				for (uint32 k = 0; k < n->sockets.Size(); ++k) {
					const graph::NkSocket &so = n->sockets[k];
					NkVec2 p;
					NkCanevasPrise(e, zone, *n, static_cast<int32>(k), s, p);
					const bool liee = PriseLiee(g, n->id, static_cast<int32>(k));
					const bool entree = so.dir == graph::NkSocketDir::Input;
					const bool exec = so.family == graph::NkSocketFamily::Exec;
					float32 ap = alpha;
					if (tirePrise != nullptr && !(tireNoeud->id == n->id && static_cast<int32>(k) == e.gestePrise) &&
						!Compatible(g, *tireNoeud, *tirePrise, *n, so)) {
						ap *= 0.3f; // pendant le tirage, l'incompatible s'eteint
					}
					const NkColor c = NkAlphaNodal(CouleurPrise(g, so, s, d), ap);
					if (exec) {
						const float32 w = s.execLargeur * z, h = s.execHauteur * z;
						const float32 x0 = entree ? r.x - s.execDehors * z : r.x + r.w - w + s.execDehors * z;
						PriseExec(dl, x0, p.y, w, h, c, NkAlphaNodal(s.corps, ap), liee);
					} else {
						PriseDonnee(dl, p.x, p.y, s.priseLargeur * z, s.priseHauteur * z, c, NkAlphaNodal(s.corps, ap), liee, s.priseContour);
					}
					if (static_cast<int32>(k) == tete) {
						continue; // l'entree d'execution de l'en-tete n'a pas d'etiquette
					}
					// L'etiquette (orange quand un fil arrive), la pastille de type.
					const NkString lib = LibellePrise(g, *n, so, d);
					const NkColor lc = NkAlphaNodal(liee || exec ? s.libelleBranche : s.texte, alpha);
					const float32 yt = p.y - lh * 0.5f;
					if (entree) {
						float32 x = r.x + s.margeNoeud * z;
						if (!exec && valeurs) {
							Pastille(dl, police, x, p.y, NkGlypheTypeNodal(NomType(g, so)), CouleurPrise(g, so, s, d), s, z, alpha);
							x += (LargeurPastille(police, NkGlypheTypeNodal(NomType(g, so))) + 6.f) * z;
						}
						Texte(dl, police, x, yt, lib.CStr(), lc, z);
						if (!exec && valeurs) {
							// Le champ (ou « branchée »), aligne a droite.
							if (liee) {
								const float32 wb = Largeur(police, "branchée", z * 0.86f);
								Texte(dl, police, r.x + r.w - s.margeNoeud * z - wb, p.y - lh * 0.86f * 0.5f, "branchée", NkAlphaNodal(s.branchee, alpha), z * 0.86f);
							} else {
								const NkGenreChamp gc = GenreChamp(g, *n, so, d);
								const bool enSaisie = e.saisieNoeud == n->id && e.saisiePrise == static_cast<int32>(k);
								const NkString v = enSaisie ? NkString(e.saisie) : ValeurLibre(g, *n, k, d);
								if (gc == NkGenreChamp::NK_AUCUN || (!enSaisie && v.Empty())) {
									continue;
								}
								const NkRect c2 = RectChamp(e, r, *n, k, s, gc);
								if (gc == NkGenreChamp::NK_CASE) {
									const bool vrai = v == "vrai" || v == "1" || v == "true";
									dl.AddRectFilled(c2, NkAlphaNodal(vrai ? CouleurPrise(g, so, s, d) : s.champ, alpha), 2.f * z);
									dl.AddRect(c2, NkAlphaNodal(s.champBord, alpha), 1.f, 2.f * z);
									if (vrai) {
										dl.AddLine(NkVec2{c2.x + c2.w * 0.22f, c2.y + c2.h * 0.52f}, NkVec2{c2.x + c2.w * 0.42f, c2.y + c2.h * 0.74f},
												   NkColor(18, 18, 18, 255), 1.8f * z);
										dl.AddLine(NkVec2{c2.x + c2.w * 0.42f, c2.y + c2.h * 0.74f}, NkVec2{c2.x + c2.w * 0.8f, c2.y + c2.h * 0.28f},
												   NkColor(18, 18, 18, 255), 1.8f * z);
									}
									continue;
								}
								dl.AddRectFilled(c2, NkAlphaNodal(s.champ, alpha), 2.f * z);
								if (enSaisie) {
									dl.AddRect(c2, s.libelleBranche, 1.f, 2.f * z);
								}
								float32 vx = c2.x + 6.f * z;
								if (gc == NkGenreChamp::NK_NUANCIER && v.Length() >= 7u && v.CStr()[0] == '#') {
									// Le NUANCIER : la couleur saisie, puis son code.
									const uint32 rvba = static_cast<uint32>(std::strtoul(v.CStr() + 1, nullptr, 16));
									const NkColor cn = v.Length() >= 9u ? NkColor(rvba) : NkColor((rvba << 8) | 0xFFu);
									dl.AddRectFilled(NkRect{c2.x + 3.f * z, c2.y + 3.f * z, 18.f * z, c2.h - 6.f * z}, cn, 1.f * z);
									vx += 20.f * z;
								}
								Texte(dl, police, vx, c2.y + (c2.h - lh * 0.86f) * 0.5f, v.CStr(), NkAlphaNodal(s.texte, alpha), z * 0.86f,
									  c2.x + c2.w - vx - 4.f * z);
								if (enSaisie) {
									const float32 cx = vx + Largeur(police, v.CStr(), z * 0.86f) + 1.f;
									dl.AddLine(NkVec2{cx, c2.y + 3.f}, NkVec2{cx, c2.y + c2.h - 3.f}, s.texte, 1.f);
								}
							}
						}
					} else {
						float32 x = r.x + r.w - s.margeNoeud * z - (exec ? s.execLargeur * z : 0.f);
						if (!exec && valeurs) {
							const float32 wp = LargeurPastille(police, NkGlypheTypeNodal(NomType(g, so))) * z;
							x -= wp;
							Pastille(dl, police, x, p.y, NkGlypheTypeNodal(NomType(g, so)), CouleurPrise(g, so, s, d), s, z, alpha);
							x -= 6.f * z;
						}
						const float32 tw = Largeur(police, lib.CStr(), z);
						Texte(dl, police, x - tw, yt, lib.CStr(), lc, z);
					}
				}
				// Le BLOC du domaine (le code d'une Expression).
				if (geo->blocH > 0.f && d.Bloc != nullptr && valeurs) {
					const NkRect rb{r.x, r.y + geo->blocY * z, r.w, geo->blocH * z};
					const bool surBloc = dedans && Dans(rb, souris) && e.geste == 0;
					if (d.Bloc(d.donnees, dl, in, police, g, n->id, rb, z, surBloc)) {
						e.modifie = true;
					}
				}
				// La raison d'une ERREUR, ECRITE dans le noeud (« lisible sans survol »).
				if (!geo->erreur.Empty()) {
					const float32 ye = r.y + geo->erreurY * z;
					const float32 wmax = r.w - 2.f * s.margeNoeud * z;
					const float32 lw = Largeur(police, geo->erreur.CStr(), z * 0.86f);
					if (lw <= wmax) {
						Texte(dl, police, r.x + s.margeNoeud * z, ye, geo->erreur.CStr(), s.erreur, z * 0.86f, wmax);
					} else {
						// Deux lignes : coupe au dernier espace qui tient.
						const char *t = geo->erreur.CStr();
						usize coupe = 0;
						for (usize q = 0; t[q] != '\0'; ++q) {
							if (t[q] == ' ' && Largeur(police, NkString(t, q).CStr(), z * 0.86f) <= wmax) {
								coupe = q;
							}
						}
						if (coupe == 0u) {
							coupe = geo->erreur.Length() / 2u;
						}
						Texte(dl, police, r.x + s.margeNoeud * z, ye, NkString(t, coupe).CStr(), s.erreur, z * 0.86f, wmax);
						Texte(dl, police, r.x + s.margeNoeud * z, ye + lh * 0.9f, t + coupe + (t[coupe] == ' ' ? 1 : 0), s.erreur, z * 0.86f, wmax);
					}
				}
			}
			if (tirePrise != nullptr) {
				NkVec2 p;
				if (NkCanevasPrise(e, zone, *tireNoeud, e.gestePrise, s, p)) {
					NkVec2 pts[kPointsFil];
					const bool sortie = tirePrise->dir == graph::NkSocketDir::Output;
					const int32 nb = sortie ? PointsFil(p, souris) : PointsFil(souris, p);
					if (sortie) {
						Courbe(p, souris, pts, nb);
					} else {
						Courbe(souris, p, pts, nb);
					}
					Pointille(dl, pts, nb, s.filTire, 2.f, 5.f, 4.f);
				}
			} else if (const graph::NkNode *fa = g.Find(e.filAttenteNoeud)) {
				NkVec2 p;
				if (e.filAttentePrise >= 0 && static_cast<uint32>(e.filAttentePrise) < fa->sockets.Size() &&
					NkCanevasPrise(e, zone, *fa, e.filAttentePrise, s, p)) {
					const NkVec2 q = NkCanevasVersEcran(e, zone, e.filAttenteX, e.filAttenteY);
					NkVec2 pts[kPointsFil];
					const bool sortie = fa->sockets[static_cast<uint32>(e.filAttentePrise)].dir == graph::NkSocketDir::Output;
					const int32 nb = sortie ? PointsFil(p, q) : PointsFil(q, p);
					if (sortie) {
						Courbe(p, q, pts, nb);
					} else {
						Courbe(q, p, pts, nb);
					}
					Pointille(dl, pts, nb, s.filTire, 2.f, 5.f, 4.f);
				}
			}
			// Le LASSO (orange reserve, pointille SUR le bord) ; le cadre qu'on dessine.
			if ((e.geste == 4 || e.geste == 7) && e.gesteBouge) {
				const NkRect r{Min(e.appuiX, souris.x), Min(e.appuiY, souris.y), Absf(souris.x - e.appuiX), Absf(souris.y - e.appuiY)};
				const NkColor c = e.geste == 4 ? s.lasso : s.cadre;
				dl.AddRectFilled(r, NkAlphaNodal(c, 0.08f));
				RectPointille(dl, r, c, 1.5f, 4.f);
			}
			// La barre d'outils verticale.
			if (e.barreOutils) {
				const uint32 nb = static_cast<uint32>(NkOutilCanevas::NK_NOMBRE);
				const NkRect &r0 = e.rectOutils[0];
				const NkRect panneau{r0.x - 4.f, r0.y - 6.f, s.outilTaille + 8.f, s.outilTaille * static_cast<float32>(nb) + 12.f};
				dl.AddRectFilled(panneau, s.outilFond, 6.f);
				dl.AddRect(panneau, s.outilBord, 1.f, 6.f);
				for (uint32 i = 0; i < nb; ++i) {
					const NkRect &rb = e.rectOutils[i];
					const bool actif = static_cast<uint32>(e.outil) == i;
					if (actif) {
						dl.AddRectFilled(rb, s.outilActif, 4.f);
					} else if (Dans(rb, souris)) {
						dl.AddRectFilled(rb, NkAlphaNodal(s.outilActif, 0.18f), 4.f);
					}
					Icone(dl, static_cast<NkOutilCanevas>(i), rb, actif ? s.outilIconeActif : s.outilIcone);
				}
			}
			// La « Portee ».
			if (!e.portee.Empty()) {
				const NkRect &r = e.rectPortee;
				dl.AddRectFilled(r, s.outilFond, 4.f);
				dl.AddRect(r, Dans(r, souris) ? s.survol : s.outilBord, 1.f, 4.f);
				const float32 l0 = Largeur(police, "Portée : ", 1.f);
				Texte(dl, police, r.x + 10.f, r.y + (r.h - HauteurLigne(police)) * 0.5f, "Portée : ", s.attenue, 1.f);
				Texte(dl, police, r.x + 10.f + l0, r.y + (r.h - HauteurLigne(police)) * 0.5f, e.portee.CStr(), s.texteEntete, 1.f);
				const float32 cx = r.x + r.w - 13.f, cy = r.y + r.h * 0.5f;
				TriangleLisse(dl, NkVec2{cx - 4.f, cy - 2.f}, NkVec2{cx + 4.f, cy - 2.f}, NkVec2{cx, cy + 3.f}, s.attenue);
			}
			// Le dernier refus, NOMME, quelques secondes.
			if (e.ageRefus < 4.f && !e.refus.Empty()) {
				Texte(dl, police, zone.x + 64.f, zone.y + zone.h - HauteurLigne(police) - 10.f, e.refus.CStr(), s.erreur, 1.f);
			}
			dl.PopClipRect();
		}

	} // namespace editorkit
} // namespace nkentseu
