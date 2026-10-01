// -----------------------------------------------------------------------------
// @File    NkStateGraphDraw.cpp
// @Brief   Le dessin et les gestes du graphe d'etats : le fond, les etats, les
//          transitions, le panneau des parametres, l'inspecteur.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
//  ⚠️ AUCUN NOMBRE DE PIXELS ICI (regle de NkTabStripDraw.cpp) : toute
//     longueur passe par `M("...")`, toute couleur par un role du style.
//
//  L'ORDRE D'UNE TRAME : la geometrie (noeuds, panneaux) -> les gestes, qui
//  changent le modele -> la geometrie RECALCULEE -> le dessin. Le dessin montre
//  donc l'etat de cette trame, et le banc vise des rectangles exacts.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkStateGraphModel.h"

#include <cmath>
#include <cstdio>

namespace nkentseu {
	namespace editorkit {

		namespace {

			float32 Abs(float32 v) {
				return v < 0.f ? -v : v;
			}

			/// Une ligne d'un panneau (parametres, animations, inspecteur) : ce qui
			/// se clique, et ce qui se lit.
			struct Ligne {
					NkGraphHit hit;
					NkString texte;		///< le libelle (ou le texte du bouton)
					NkString valeur;	///< la valeur montree a droite (vide = aucune)
					uint16 pastille = 0; ///< role d'une pastille a gauche (0 = aucune)
					bool bouton = false;  ///< dessinee comme un bouton
					bool titre = false;	  ///< un en-tete de section
					bool actif = true;
					bool coche = false;	  ///< une case cochee (booleen)
			};

			struct Arc {
					nk_uint64 id = 0; ///< la transition (0 = la fleche d'entree)
					float32 ax = 0.f, ay = 0.f, bx = 0.f, by = 0.f;
					bool entree = false;
			};

			struct Ctx {
					NkComponentPaint &p;
					const NkComponentInput &in;
					NkStateGraphModel &m;
					const NkStateGraphStyle &s;
					NkStateGraphResult &r;
					float32 S = 1.f;
					NkPaintRect bar, side, inspector, canvas;
					NkVector<Ligne> lignes;
					NkVector<Arc> arcs;

					float32 M(const char *n) const {
						return NkStateGraphMetric(s, n) * S;
					}
					float32 Raw(const char *n) const {
						return NkStateGraphMetric(s, n);
					}
					bool Over(const NkPaintRect &rc) const {
						return rc.w > 0.f && rc.Contains(in.mouseX, in.mouseY);
					}
			};

			uint32 Voile(NkComponentPaint &p, uint16 role, uint32 alpha) {
				return (p.ColorOf(role) & 0xFFFFFF00u) | (alpha & 0xFFu);
			}

			void Triangle(Ctx &c, float32 x0, float32 y0, float32 x1, float32 y1, float32 x2, float32 y2, uint16 role) {
				const float32 xy[6] = {x0, y0, x1, y1, x2, y2};
				if (!c.p.PolygonHex(xy, 3, c.p.ColorOf(role))) {
					c.p.Line(x0, y0, x1, y1, role, c.M("stroke_w"));
					c.p.Line(x1, y1, x2, y2, role, c.M("stroke_w"));
					c.p.Line(x2, y2, x0, y0, role, c.M("stroke_w"));
				}
			}

			// ── LA GEOMETRIE ────────────────────────────────────────────────────
			void Planifier(Ctx &c, const NkPaintRect &rect) {
				float32 sw = c.M("side_w"), iw = c.M("inspector_w");
				if (sw + iw > rect.w * 0.6f) {
					const float32 k = rect.w * 0.6f / (sw + iw);
					sw *= k;
					iw *= k;
				}
				c.side = NkPaintRect{rect.x, rect.y, sw, rect.h};
				c.inspector = NkPaintRect{rect.x + rect.w - iw, rect.y, iw, rect.h};
				const float32 cx = rect.x + sw + c.M("stroke_w");
				const float32 cw = rect.w - sw - iw - c.M("stroke_w") * 2.f;
				const float32 bh = c.M("bar_h");
				c.bar = NkPaintRect{cx, rect.y, cw > 0.f ? cw : 0.f, bh};
				c.canvas = NkPaintRect{cx, rect.y + bh, cw > 0.f ? cw : 0.f, rect.h - bh > 0.f ? rect.h - bh : 0.f};
				c.r.side = c.side;
				c.r.inspector = c.inspector;
				c.r.canvas = c.canvas;
			}

			/// Le rectangle a l'ecran d'un noeud (etat ou pseudo) du niveau regarde.
			NkPaintRect RectNoeud(Ctx &c, nk_uint64 id) {
				NkStateGraphModel &m = c.m;
				const float32 z = m.zoom;
				float32 gx = 0.f, gy = 0.f, w = c.M("node_w") * z, h = c.M("node_h") * z;
				if (id == kNkGraphEntryId || id == kNkGraphAnyId || id == kNkGraphUpId) {
					const NkGraphPseudo &p = m.PseudoOf(m.level);
					w = c.M("pseudo_w") * z;
					h = c.M("pseudo_h") * z;
					gx = id == kNkGraphAnyId ? p.anyX : p.entryX;
					gy = id == kNkGraphAnyId ? p.anyY : (id == kNkGraphUpId ? p.entryY - 90.f : p.entryY);
				} else if (const NkGraphNode *n = m.Node(id)) {
					gx = n->x;
					gy = n->y;
				}
				float32 sx = 0.f, sy = 0.f;
				NkStateGraphToScreen(m, c.canvas, c.S, gx, gy, sx, sy);
				return NkPaintRect{sx - w * 0.5f, sy - h * 0.5f, w, h};
			}

			NkPaintRect Port(Ctx &c, const NkPaintRect &n) {
				const float32 pr = c.M("port_r") * (c.m.zoom < 1.f ? 1.f : c.m.zoom);
				return NkPaintRect{n.x + n.w - pr, n.y + n.h * 0.5f - pr, pr * 2.f, pr * 2.f};
			}

			void Noeuds(Ctx &c) {
				NkStateGraphModel &m = c.m;
				c.r.nodeRects.Clear();
				auto ajouter = [&](nk_uint64 id) {
					NkGraphNodeRect nr;
					nr.id = id;
					nr.rect = RectNoeud(c, id);
					nr.port = Port(c, nr.rect);
					c.r.nodeRects.PushBack(nr);
				};
				ajouter(kNkGraphEntryId);
				ajouter(kNkGraphAnyId);
				if (m.level != 0) {
					ajouter(kNkGraphUpId);
				}
				for (uint32 i = 0; i < (uint32)m.nodes.Size(); ++i) {
					if (m.nodes[i].parent == m.level) {
						ajouter(m.nodes[i].id);
					}
				}
			}

			const NkGraphNodeRect *RectDe(const Ctx &c, nk_uint64 id) {
				for (uint32 i = 0; i < (uint32)c.r.nodeRects.Size(); ++i) {
					if (c.r.nodeRects[i].id == id) {
						return &c.r.nodeRects[i];
					}
				}
				return nullptr;
			}

			/// Le noeud sous le curseur (le dernier dessine l'emporte : il est dessus).
			nk_uint64 NoeudSous(const Ctx &c, bool *surPort = nullptr) {
				for (int32 i = (int32)c.r.nodeRects.Size() - 1; i >= 0; --i) {
					const NkGraphNodeRect &nr = c.r.nodeRects[(uint32)i];
					const bool port = nr.id != kNkGraphUpId && c.Over(nr.port);
					if (c.Over(nr.rect) || port) {
						if (surPort != nullptr) {
							*surPort = port;
						}
						return nr.id;
					}
				}
				return 0;
			}

			/// Les fleches du niveau regarde (voir l'en-tete du modele : chaque bout
			/// est REPRESENTE par l'etat de ce niveau qui le contient, ou par « ↑ »).
			void Arcs(Ctx &c) {
				NkStateGraphModel &m = c.m;
				c.arcs.Clear();
				const float32 off = c.M("wire_offset");
				auto centre = [&](nk_uint64 id, float32 &x, float32 &y) {
					const NkGraphNodeRect *nr = RectDe(c, id);
					if (nr == nullptr) {
						return false;
					}
					x = nr->rect.x + nr->rect.w * 0.5f;
					y = nr->rect.y + nr->rect.h * 0.5f;
					return true;
				};
				NkVector<nk_uint64> paires; // (a, b) deja poses, pour ecarter les suivants
				auto poser = [&](nk_uint64 tid, nk_uint64 a, nk_uint64 b, bool entree) {
					Arc arc;
					arc.id = tid;
					arc.entree = entree;
					if (!centre(a, arc.ax, arc.ay) || !centre(b, arc.bx, arc.by)) {
						return;
					}
					// A->B et B->A cote a cote : un decalage perpendiculaire ; la
					// n-ieme fleche d'une meme paire s'ecarte d'autant (un niveau
					// parent en porte souvent plusieurs vers le meme etat).
					uint32 rang = 0;
					for (uint32 k = 0; k + 1 < (uint32)paires.Size(); k += 2) {
						rang += (paires[k] == a && paires[k + 1] == b) ? 1u : 0u;
					}
					paires.PushBack(a);
					paires.PushBack(b);
					const float32 dx = arc.bx - arc.ax, dy = arc.by - arc.ay;
					const float32 l = std::sqrt(dx * dx + dy * dy);
					if (l < 1e-3f) {
						return;
					}
					const float32 ecart = off * (1.f + 2.2f * (float32)rang);
					const float32 nx = -dy / l * ecart, ny = dx / l * ecart;
					arc.ax += nx;
					arc.ay += ny;
					arc.bx += nx;
					arc.by += ny;
					c.arcs.PushBack(arc);
				};
				const nk_uint64 entree = m.EntryOf(m.level);
				if (entree != 0) {
					poser(0, kNkGraphEntryId, entree, true);
				}
				for (uint32 i = 0; i < (uint32)m.transitions.Size(); ++i) {
					const NkGraphTransition &t = m.transitions[i];
					nk_uint64 a = 0, b = m.RepresentativeAt(t.to, m.level);
					if (t.any) {
						if (t.scope != m.level) {
							continue;
						}
						a = kNkGraphAnyId;
					} else {
						a = m.RepresentativeAt(t.from, m.level);
					}
					if (a == 0 && b == 0) {
						continue;
					}
					if (m.level == 0 && (a == 0 || b == 0)) {
						continue;
					}
					a = a == 0 ? kNkGraphUpId : a;
					b = b == 0 ? kNkGraphUpId : b;
					if (a == b) {
						continue; // interne a une sous-machine de ce niveau
					}
					poser(t.id, a, b, false);
				}
			}

			float32 DistanceSegment(float32 px, float32 py, const Arc &a) {
				const float32 dx = a.bx - a.ax, dy = a.by - a.ay;
				const float32 l2 = dx * dx + dy * dy;
				float32 u = l2 > 1e-6f ? ((px - a.ax) * dx + (py - a.ay) * dy) / l2 : 0.f;
				u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
				const float32 qx = a.ax + dx * u - px, qy = a.ay + dy * u - py;
				return std::sqrt(qx * qx + qy * qy);
			}

			nk_uint64 ArcSous(const Ctx &c) {
				float32 best = c.M("hit_wire");
				nk_uint64 id = 0;
				for (uint32 i = 0; i < (uint32)c.arcs.Size(); ++i) {
					const Arc &a = c.arcs[i];
					if (a.entree) {
						continue;
					}
					const float32 d = DistanceSegment(c.in.mouseX, c.in.mouseY, a);
					if (d <= best) {
						best = d;
						id = a.id;
					}
				}
				return id;
			}

			// ── LES PANNEAUX : des lignes, construites avant les gestes ───────────
			void PlacerBoutons(Ctx &c) {
				const float32 pad = c.M("pad");
				const float32 gap = c.M("button_gap");
				const float32 by = c.bar.y + gap;
				const float32 bh = c.bar.h - gap * 2.f;
				static const char *const kTextes[] = {"+ Etat", "+ Sous-machine", "Relier", "Supprimer",
													  "Annuler", "Refaire", "Cadrer"};
				// De DROITE a gauche : le fil d'Ariane garde la gauche de la barre.
				float32 x = c.bar.x + c.bar.w - pad;
				for (int32 b = (int32)NkStateGraphButton::Frame; b >= 0; --b) {
					const float32 w = c.p.TextWidth(kTextes[b]) + pad * 2.f;
					x -= w;
					c.r.buttons[b] = NkPaintRect{x, by, w, bh};
					if (x < c.bar.x + c.bar.w * 0.35f) {
						c.r.buttons[b].w = 0.f; // la barre est trop etroite : bouton cache
					}
					x -= gap;
				}
				// Les trois « + » des parametres, en tete du panneau de gauche.
				const float32 rh = c.M("row_h");
				const float32 w3 = (c.side.w - pad * 2.f - gap * 2.f) / 3.f;
				for (int32 k = 0; k < 3; ++k) {
					c.r.buttons[(uint8)NkStateGraphButton::AddBool + k] =
						NkPaintRect{c.side.x + pad + (w3 + gap) * (float32)k, c.side.y + rh + gap, w3, rh - gap * 2.f};
				}
			}

			void FilAriane(Ctx &c) {
				NkStateGraphModel &m = c.m;
				NkVector<nk_uint64> niveaux;
				for (const NkGraphNode *n = m.Node(m.level); n != nullptr; n = m.Node(n->parent)) {
					niveaux.Insert(niveaux.Begin(), n->id);
					if (niveaux.Size() > 32) {
						break;
					}
				}
				niveaux.Insert(niveaux.Begin(), (nk_uint64)0);
				const float32 pad = c.M("pad");
				float32 x = c.bar.x + pad;
				for (uint32 k = 0; k < (uint32)niveaux.Size(); ++k) {
					const NkGraphNode *n = m.Node(niveaux[k]);
					Ligne l;
					l.texte = n != nullptr ? n->name : NkString("Racine");
					const float32 w = c.p.TextWidth(l.texte.CStr()) + pad;
					l.hit.field = NkGraphField::Breadcrumb;
					l.hit.index = (int32)k;
					l.hit.id = niveaux[k];
					l.hit.rect = NkPaintRect{x, c.bar.y, w, c.bar.h};
					l.actif = k + 1 < (uint32)niveaux.Size(); // le dernier est le niveau courant
					c.lignes.PushBack(l);
					x += w + c.p.TextWidth(" > ");
				}
			}

			void PanneauParametres(Ctx &c) {
				NkStateGraphModel &m = c.m;
				const float32 pad = c.M("pad");
				const float32 rh = c.M("row_h");
				float32 y = c.side.y;
				Ligne t;
				t.titre = true;
				t.texte = "Parametres";
				t.hit.rect = NkPaintRect{c.side.x, y, c.side.w, rh};
				c.lignes.PushBack(t);
				y += rh * 2.f; // la ligne des trois « + »
				for (uint32 i = 0; i < (uint32)m.params.Size(); ++i) {
					const NkGraphParam &p = m.params[i];
					const float32 vw = c.side.w * 0.32f;
					const float32 xw = rh;
					Ligne nom;
					nom.hit.field = NkGraphField::ParamName;
					nom.hit.index = (int32)i;
					nom.hit.rect = NkPaintRect{c.side.x + pad, y, c.side.w - pad * 2.f - vw - xw, rh};
					nom.texte = p.name;
					nom.pastille = p.kind == (uint8)NkGraphParamKind::Bool
									   ? c.s.paramBool
									   : (p.kind == (uint8)NkGraphParamKind::Trigger ? c.s.paramTrigger : c.s.paramFloat);
					c.lignes.PushBack(nom);
					Ligne val;
					val.hit.field = NkGraphField::ParamValue;
					val.hit.index = (int32)i;
					val.hit.rect = NkPaintRect{c.side.x + c.side.w - pad - xw - vw, y + c.M("button_gap"), vw,
											   rh - c.M("button_gap") * 2.f};
					const float32 v = m.live ? p.value : p.defaultValue;
					char txt[24];
					if (p.kind == (uint8)NkGraphParamKind::Float) {
						std::snprintf(txt, sizeof(txt), "%.2f", (double)v);
						val.valeur = txt;
					} else if (p.kind == (uint8)NkGraphParamKind::Bool) {
						val.coche = v > 0.5f;
						val.valeur = val.coche ? "vrai" : "faux";
					} else {
						val.bouton = true;
						val.coche = v > 0.5f;
						val.valeur = "Tirer";
					}
					c.lignes.PushBack(val);
					Ligne x;
					x.hit.field = NkGraphField::ParamRemove;
					x.hit.index = (int32)i;
					x.hit.rect = NkPaintRect{c.side.x + c.side.w - pad - xw, y, xw, rh};
					x.texte = "-";
					c.lignes.PushBack(x);
					y += rh;
				}
				y += rh * 0.5f;
				Ligne ta;
				ta.titre = true;
				ta.texte = "Animations";
				ta.hit.rect = NkPaintRect{c.side.x, y, c.side.w, rh};
				c.lignes.PushBack(ta);
				y += rh;
				if (m.clips.Empty()) {
					Ligne aucune;
					aucune.texte = "(aucune animation)";
					aucune.actif = false;
					aucune.hit.rect = NkPaintRect{c.side.x + pad, y, c.side.w - pad * 2.f, rh};
					c.lignes.PushBack(aucune);
				}
				for (uint32 i = 0; i < (uint32)m.clips.Size(); ++i) {
					Ligne l;
					l.hit.field = NkGraphField::ClipItem;
					l.hit.index = (int32)i;
					l.hit.rect = NkPaintRect{c.side.x + pad, y, c.side.w - pad * 2.f, rh};
					l.texte = m.clips[i];
					l.valeur = "+ etat";
					c.lignes.PushBack(l);
					y += rh;
				}
			}

			void Champ(Ctx &c, float32 &y, NkGraphField f, const char *libelle, const NkString &valeur, nk_uint64 id,
					   int32 index = -1, bool bouton = false, bool actif = true) {
				const float32 pad = c.M("pad");
				const float32 rh = c.M("row_h");
				Ligne l;
				l.hit.field = f;
				l.hit.id = id;
				l.hit.index = index;
				l.texte = libelle;
				l.valeur = valeur;
				l.bouton = bouton;
				l.actif = actif;
				if (bouton) {
					l.hit.rect = NkPaintRect{c.inspector.x + pad, y + c.M("button_gap"), c.inspector.w - pad * 2.f,
											 rh - c.M("button_gap") * 2.f};
				} else {
					// Le libelle a gauche, la VALEUR (la zone qui se clique) a droite.
					const float32 lw = (c.inspector.w - pad * 2.f) * 0.42f;
					l.hit.rect = NkPaintRect{c.inspector.x + pad + lw, y + c.M("button_gap"), c.inspector.w - pad * 2.f - lw,
											 rh - c.M("button_gap") * 2.f};
				}
				c.lignes.PushBack(l);
				y += rh;
			}

			void Info(Ctx &c, float32 &y, const NkString &texte, bool titre = false) {
				Ligne l;
				l.titre = titre;
				l.texte = texte;
				l.actif = !titre;
				l.hit.rect = NkPaintRect{c.inspector.x + c.M("pad"), y, c.inspector.w - c.M("pad") * 2.f, c.M("row_h")};
				c.lignes.PushBack(l);
				y += c.M("row_h");
			}

			void Inspecteur(Ctx &c) {
				NkStateGraphModel &m = c.m;
				float32 y = c.inspector.y;
				char txt[64];
				if (const NkGraphTransition *t = m.Transition(m.selectedTransition)) {
					Info(c, y, "Transition", true);
					NkString de = t->any ? NkString("N'importe quel etat") : m.PathOf(t->from);
					de.Append("  ->  ");
					de.Append(m.PathOf(t->to));
					Info(c, y, de);
					std::snprintf(txt, sizeof(txt), "%.2f s", (double)t->fade);
					Champ(c, y, NkGraphField::TransFade, "Fondu", txt, t->id);
					std::snprintf(txt, sizeof(txt), "%d", t->priority);
					Champ(c, y, NkGraphField::TransPriority, "Priorite", txt, t->id);
					Info(c, y, "Conditions (toutes vraies)", true);
					if (t->conditions.Empty()) {
						Info(c, y, "aucune : tire des que l'etat est actif");
					}
					const float32 pad = c.M("pad");
					const float32 rh = c.M("row_h");
					const float32 gap = c.M("button_gap");
					const float32 w = c.inspector.w - pad * 2.f;
					for (uint32 k = 0; k < (uint32)t->conditions.Size(); ++k) {
						const NkGraphCondition &cd = t->conditions[k];
						const float32 wp = w * 0.42f, wo = w * 0.2f, wv = w * 0.26f, wx = w - wp - wo - wv;
						float32 x = c.inspector.x + pad;
						Ligne lp;
						lp.hit.field = NkGraphField::CondParam;
						lp.hit.id = t->id;
						lp.hit.index = (int32)k;
						lp.hit.rect = NkPaintRect{x, y + gap, wp - gap, rh - gap * 2.f};
						lp.valeur = cd.op == (uint8)NkGraphCondOp::TimeInState ? NkString("(temps)") : cd.param;
						c.lignes.PushBack(lp);
						x += wp;
						Ligne lo;
						lo.hit.field = NkGraphField::CondOp;
						lo.hit.id = t->id;
						lo.hit.index = (int32)k;
						lo.hit.rect = NkPaintRect{x, y + gap, wo - gap, rh - gap * 2.f};
						lo.valeur = NkGraphCondOpName(cd.op);
						c.lignes.PushBack(lo);
						x += wo;
						const bool seuil = cd.op == (uint8)NkGraphCondOp::Greater || cd.op == (uint8)NkGraphCondOp::Less ||
										   cd.op == (uint8)NkGraphCondOp::TimeInState;
						if (seuil) {
							Ligne lv;
							lv.hit.field = NkGraphField::CondValue;
							lv.hit.id = t->id;
							lv.hit.index = (int32)k;
							lv.hit.rect = NkPaintRect{x, y + gap, wv - gap, rh - gap * 2.f};
							std::snprintf(txt, sizeof(txt), "%.2f", (double)cd.threshold);
							lv.valeur = txt;
							c.lignes.PushBack(lv);
						}
						x += wv;
						Ligne lx;
						lx.hit.field = NkGraphField::CondRemove;
						lx.hit.id = t->id;
						lx.hit.index = (int32)k;
						lx.hit.rect = NkPaintRect{x, y + gap, wx, rh - gap * 2.f};
						lx.texte = "-";
						lx.bouton = true;
						c.lignes.PushBack(lx);
						y += rh;
					}
					Champ(c, y, NkGraphField::CondAdd, "+ Condition", NkString(), t->id, -1, true, !m.params.Empty());
					if (m.params.Empty()) {
						Info(c, y, "(ajoutez d'abord un parametre)");
					}
					y += c.M("row_h") * 0.5f;
					Champ(c, y, NkGraphField::TransDelete, "Supprimer la transition", NkString(), t->id, -1, true);
					return;
				}
				if (m.selectedNode == kNkGraphEntryId || m.selectedNode == kNkGraphAnyId) {
					Info(c, y, m.selectedNode == kNkGraphEntryId ? "Entree" : "N'importe quel etat", true);
					Info(c, y, m.selectedNode == kNkGraphEntryId ? "Glisser depuis son rond vers un etat :"
																  : "Glisser depuis son rond vers un etat :");
					Info(c, y, m.selectedNode == kNkGraphEntryId ? "il devient l'etat d'entree du niveau."
																  : "une transition depuis tout le niveau.");
					return;
				}
				if (const NkGraphNode *n = m.Node(m.selectedNode)) {
					Info(c, y, n->subMachine ? "Sous-machine" : "Etat", true);
					Champ(c, y, NkGraphField::NodeName, "Nom", n->name, n->id);
					if (!n->subMachine) {
						Champ(c, y, NkGraphField::NodeClip, "Animation", n->clip.Empty() ? NkString("(aucune)") : n->clip,
							  n->id);
						std::snprintf(txt, sizeof(txt), "%d", n->tag);
						Champ(c, y, NkGraphField::NodeTag, "Etiquette", txt, n->id);
					}
					const bool entree = m.EntryOf(n->parent) == n->id;
					Champ(c, y, NkGraphField::NodeSetEntry, entree ? "Etat d'entree du niveau" : "Definir comme entree",
						  NkString(), n->id, -1, true, !entree);
					if (n->subMachine) {
						Champ(c, y, NkGraphField::NodeEnter, "Ouvrir la sous-machine", NkString(), n->id, -1, true);
					}
					NkString chemin("Chemin : ");
					chemin.Append(m.PathOf(n->id));
					Info(c, y, chemin);
					y += c.M("row_h") * 0.5f;
					Champ(c, y, NkGraphField::NodeDelete, n->subMachine ? "Supprimer la sous-machine" : "Supprimer l'etat",
						  NkString(), n->id, -1, true);
					return;
				}
				Info(c, y, "Graphe d'etats", true);
				Info(c, y, "Double-clic sur le fond : un etat.");
				Info(c, y, "Glisser depuis le rond d'un etat :");
				Info(c, y, "une transition (ou Maj + glisser).");
				Info(c, y, "Double-clic sur une sous-machine :");
				Info(c, y, "l'ouvrir. Molette : zoom.");
				std::snprintf(txt, sizeof(txt), "%u etat(s), %u transition(s)", (uint32)m.nodes.Size(),
							  (uint32)m.transitions.Size());
				Info(c, y, txt);
			}

			void Construire(Ctx &c) {
				c.lignes.Clear();
				Noeuds(c);
				Arcs(c);
				PlacerBoutons(c);
				FilAriane(c);
				PanneauParametres(c);
				Inspecteur(c);
			}

			const Ligne *LigneSous(const Ctx &c) {
				for (uint32 i = 0; i < (uint32)c.lignes.Size(); ++i) {
					const Ligne &l = c.lignes[i];
					if (l.hit.field != NkGraphField::None && l.actif && c.Over(l.hit.rect)) {
						return &l;
					}
				}
				return nullptr;
			}

			// ── LES GESTES ──────────────────────────────────────────────────────
			NkString ClipSuivant(const NkStateGraphModel &m, const NkString &courant) {
				if (m.clips.Empty()) {
					return NkString();
				}
				if (courant.Empty()) {
					return m.clips[0];
				}
				for (uint32 i = 0; i < (uint32)m.clips.Size(); ++i) {
					if (m.clips[i] == courant) {
						return i + 1 < (uint32)m.clips.Size() ? m.clips[i + 1] : NkString();
					}
				}
				return m.clips[0];
			}

			void Selectionner(Ctx &c, nk_uint64 noeud, nk_uint64 transition) {
				if (c.m.selectedNode != noeud || c.m.selectedTransition != transition) {
					c.r.selectionChanged = true;
				}
				c.m.selectedNode = noeud;
				c.m.selectedTransition = transition;
			}

			void ChangerNiveau(Ctx &c, nk_uint64 niveau) {
				NkStateGraphModel &m = c.m;
				if (m.level == niveau) {
					return;
				}
				m.level = niveau;
				Selectionner(c, 0, 0);
				m.FrameLevel(c.canvas.w / c.S, c.canvas.h / c.S);
				c.r.levelChanged = true;
			}

			void ParamValeur(Ctx &c, uint32 i, float32 v) {
				NkStateGraphModel &m = c.m;
				NkGraphParam &p = m.params[i];
				if (m.live) {
					p.value = v;
					c.r.paramValueChanged = true;
					c.r.paramIndex = (int32)i;
				} else {
					p.defaultValue = v;
					p.value = v;
					m.Touch();
					c.r.changed = true;
				}
			}

			void ActionLigne(Ctx &c, const Ligne &l) {
				NkStateGraphModel &m = c.m;
				switch (l.hit.field) {
					case NkGraphField::Breadcrumb:
						ChangerNiveau(c, l.hit.id);
						break;
					case NkGraphField::ParamValue: {
						NkGraphParam &p = m.params[(uint32)l.hit.index];
						if (p.kind == (uint8)NkGraphParamKind::Bool) {
							if (!m.live) {
								m.PushUndo();
							}
							ParamValeur(c, (uint32)l.hit.index, (m.live ? p.value : p.defaultValue) > 0.5f ? 0.f : 1.f);
						} else if (p.kind == (uint8)NkGraphParamKind::Trigger) {
							if (m.live) {
								p.value = 1.f;
								c.r.triggerFired = true;
								c.r.paramValueChanged = true;
								c.r.paramIndex = l.hit.index;
							}
						} else {
							m.gesture = (uint8)NkStateGraphGesture::ScrubParam;
							m.gestureParam = l.hit.index;
							m.gestureValue = m.live ? p.value : p.defaultValue;
						}
						break;
					}
					case NkGraphField::ParamRemove:
						m.RemoveParam((uint32)l.hit.index);
						c.r.changed = true;
						break;
					case NkGraphField::ClipItem: {
						// Un etat qui JOUE cette animation, au centre de la vue, un peu
						// decale a chaque fois pour ne pas empiler.
						const float32 decal = (float32)(m.nodes.Size() % 5) * 24.f;
						const nk_uint64 id = m.AddState(m.clips[(uint32)l.hit.index], m.level, m.panX + decal, m.panY + decal,
														m.clips[(uint32)l.hit.index]);
						Selectionner(c, id, 0);
						c.r.changed = true;
						break;
					}
					case NkGraphField::NodeClip:
						if (NkGraphNode *n = m.Node(l.hit.id)) {
							m.PushUndo();
							n = m.Node(l.hit.id);
							n->clip = ClipSuivant(m, n->clip);
							m.Touch();
							c.r.changed = true;
						}
						break;
					case NkGraphField::NodeTag:
					case NkGraphField::TransFade:
					case NkGraphField::TransPriority:
					case NkGraphField::CondValue: {
						m.gesture = (uint8)NkStateGraphGesture::ScrubField;
						m.gestureParam = (int32)l.hit.field;
						m.gestureNode = l.hit.id;
						m.gestureCondition = l.hit.index;
						float32 v = 0.f;
						if (const NkGraphNode *n = m.Node(l.hit.id)) {
							v = (float32)n->tag;
						}
						if (const NkGraphTransition *t = m.Transition(l.hit.id)) {
							v = l.hit.field == NkGraphField::TransFade
									? t->fade
									: (l.hit.field == NkGraphField::TransPriority
										   ? (float32)t->priority
										   : (l.hit.index >= 0 && l.hit.index < (int32)t->conditions.Size()
												  ? t->conditions[(uint32)l.hit.index].threshold
												  : 0.f));
						}
						m.gestureValue = v;
						break;
					}
					case NkGraphField::NodeSetEntry:
						if (const NkGraphNode *n = m.Node(l.hit.id)) {
							m.SetEntry(n->parent, n->id);
							c.r.changed = true;
						}
						break;
					case NkGraphField::NodeEnter:
						ChangerNiveau(c, l.hit.id);
						break;
					case NkGraphField::NodeDelete:
						m.RemoveNode(l.hit.id);
						Selectionner(c, 0, 0);
						c.r.changed = true;
						break;
					case NkGraphField::CondParam: {
						NkGraphTransition *t = m.Transition(l.hit.id);
						if (t == nullptr || m.params.Empty()) {
							break;
						}
						m.PushUndo();
						t = m.Transition(l.hit.id);
						NkGraphCondition &cd = t->conditions[(uint32)l.hit.index];
						const int32 pi = m.ParamIndex(cd.param);
						const uint32 suivant = pi < 0 ? 0u : ((uint32)pi + 1u) % (uint32)m.params.Size();
						cd.param = m.params[suivant].name;
						if (cd.op != (uint8)NkGraphCondOp::TimeInState) {
							cd.op = NkStateGraphModel::DefaultOp(m.params[suivant].kind);
						}
						m.Touch();
						c.r.changed = true;
						break;
					}
					case NkGraphField::CondOp: {
						NkGraphTransition *t = m.Transition(l.hit.id);
						if (t == nullptr) {
							break;
						}
						m.PushUndo();
						t = m.Transition(l.hit.id);
						NkGraphCondition &cd = t->conditions[(uint32)l.hit.index];
						const int32 pi = m.ParamIndex(cd.param);
						const uint8 kind = pi >= 0 ? m.params[(uint32)pi].kind : (uint8)NkGraphParamKind::Float;
						cd.op = NkStateGraphModel::NextOp(kind, cd.op);
						m.Touch();
						c.r.changed = true;
						break;
					}
					case NkGraphField::CondRemove:
						m.RemoveCondition(l.hit.id, (uint32)l.hit.index);
						c.r.changed = true;
						break;
					case NkGraphField::CondAdd:
						m.AddCondition(l.hit.id);
						c.r.changed = true;
						break;
					case NkGraphField::TransDelete:
						m.RemoveTransition(l.hit.id);
						Selectionner(c, 0, 0);
						c.r.changed = true;
						break;
					default:
						break;
				}
			}

			void ActionBouton(Ctx &c, NkStateGraphButton b) {
				NkStateGraphModel &m = c.m;
				switch (b) {
					case NkStateGraphButton::AddState:
					case NkStateGraphButton::AddSubMachine: {
						const float32 decal = (float32)(m.nodes.Size() % 5) * 24.f;
						const nk_uint64 id = b == NkStateGraphButton::AddState
												 ? m.AddState("Etat", m.level, m.panX + decal, m.panY + decal)
												 : m.AddSubMachine("Sous-machine", m.level, m.panX + decal, m.panY + decal);
						Selectionner(c, id, 0);
						c.r.changed = true;
						break;
					}
					case NkStateGraphButton::Link:
						m.linkMode = !m.linkMode;
						break;
					case NkStateGraphButton::Delete:
						if (m.selectedTransition != 0) {
							m.RemoveTransition(m.selectedTransition);
							c.r.changed = true;
						} else if (m.Node(m.selectedNode) != nullptr) {
							m.RemoveNode(m.selectedNode);
							c.r.changed = true;
						}
						Selectionner(c, 0, 0);
						break;
					case NkStateGraphButton::Undo:
						c.r.changed = m.Undo() || c.r.changed;
						break;
					case NkStateGraphButton::Redo:
						c.r.changed = m.Redo() || c.r.changed;
						break;
					case NkStateGraphButton::Frame:
						m.FrameLevel(c.canvas.w / c.S, c.canvas.h / c.S);
						break;
					case NkStateGraphButton::AddBool:
					case NkStateGraphButton::AddFloat:
					case NkStateGraphButton::AddTrigger:
						m.AddParam(NkString(), (uint8)((uint8)b - (uint8)NkStateGraphButton::AddBool),
								   b == NkStateGraphButton::AddBool ? 0.f : 0.f);
						c.r.changed = true;
						break;
					default:
						break;
				}
			}

			void Appui(Ctx &c) {
				NkStateGraphModel &m = c.m;
				const NkComponentInput &in = c.in;
				m.gestureX0 = in.mouseX;
				m.gestureY0 = in.mouseY;
				m.gestureMoved = false;
				m.gestureSnapshot = false;
				for (uint8 b = 0; b < (uint8)NkStateGraphButton::Count; ++b) {
					if (c.Over(c.r.buttons[b])) {
						ActionBouton(c, (NkStateGraphButton)b);
						return;
					}
				}
				if (const Ligne *l = LigneSous(c)) {
					const Ligne copie = *l; // l'action peut reconstruire les lignes
					ActionLigne(c, copie);
					return;
				}
				if (!c.Over(c.canvas)) {
					return;
				}
				bool surPort = false;
				const nk_uint64 n = NoeudSous(c, &surPort);
				if (n != 0) {
					Selectionner(c, n == kNkGraphUpId ? 0 : n, 0);
					const bool relier = surPort || in.shift || m.linkMode;
					if (relier && n != kNkGraphUpId) {
						m.gesture = (uint8)NkStateGraphGesture::Link;
						m.gestureNode = n;
						return;
					}
					m.gesture = (uint8)NkStateGraphGesture::MoveNode;
					m.gestureNode = n;
					if (n == kNkGraphEntryId || n == kNkGraphAnyId) {
						const NkGraphPseudo &p = m.PseudoOf(m.level);
						m.gestureA = n == kNkGraphEntryId ? p.entryX : p.anyX;
						m.gestureB = n == kNkGraphEntryId ? p.entryY : p.anyY;
					} else if (const NkGraphNode *nn = m.Node(n)) {
						m.gestureA = nn->x;
						m.gestureB = nn->y;
					}
					return;
				}
				const nk_uint64 t = ArcSous(c);
				if (t != 0) {
					Selectionner(c, 0, t);
					return;
				}
				Selectionner(c, 0, 0);
				m.gesture = (uint8)NkStateGraphGesture::Pan;
				m.gestureA = m.panX;
				m.gestureB = m.panY;
			}

			void Tenu(Ctx &c) {
				NkStateGraphModel &m = c.m;
				const NkComponentInput &in = c.in;
				const float32 dx = in.mouseX - m.gestureX0, dy = in.mouseY - m.gestureY0;
				const float32 seuil = c.M("drag_px");
				if (!m.gestureMoved && dx * dx + dy * dy > seuil * seuil) {
					m.gestureMoved = true;
				}
				if (!m.gestureMoved) {
					return;
				}
				const float32 z = m.zoom * c.S > 1e-4f ? m.zoom * c.S : 1e-4f;
				switch ((NkStateGraphGesture)m.gesture) {
					case NkStateGraphGesture::MoveNode: {
						if (!m.gestureSnapshot) {
							m.PushUndo();
							m.gestureSnapshot = true;
						}
						const float32 gx = m.gestureA + dx / z, gy = m.gestureB + dy / z;
						if (m.gestureNode == kNkGraphEntryId || m.gestureNode == kNkGraphAnyId) {
							NkGraphPseudo &p = m.PseudoOf(m.level);
							(m.gestureNode == kNkGraphEntryId ? p.entryX : p.anyX) = gx;
							(m.gestureNode == kNkGraphEntryId ? p.entryY : p.anyY) = gy;
						} else if (NkGraphNode *n = m.Node(m.gestureNode)) {
							n->x = gx;
							n->y = gy;
						}
						m.Touch();
						c.r.changed = true;
						break;
					}
					case NkStateGraphGesture::Pan:
						m.panX = m.gestureA - dx / z;
						m.panY = m.gestureB - dy / z;
						break;
					case NkStateGraphGesture::ScrubParam:
						if (m.gestureParam >= 0 && m.gestureParam < (int32)m.params.Size()) {
							if (!m.live && !m.gestureSnapshot) {
								m.PushUndo();
								m.gestureSnapshot = true;
							}
							ParamValeur(c, (uint32)m.gestureParam, m.gestureValue + dx * c.Raw("scrub_px"));
						}
						break;
					case NkStateGraphGesture::ScrubField: {
						if (!m.gestureSnapshot) {
							m.PushUndo();
							m.gestureSnapshot = true;
						}
						const NkGraphField f = (NkGraphField)m.gestureParam;
						if (f == NkGraphField::NodeTag) {
							if (NkGraphNode *n = m.Node(m.gestureNode)) {
								const int32 v = (int32)std::floor(m.gestureValue + dx * c.Raw("scrub_px") * 10.f + 0.5f);
								n->tag = v < 0 ? 0 : v;
							}
						} else if (NkGraphTransition *t = m.Transition(m.gestureNode)) {
							if (f == NkGraphField::TransFade) {
								const float32 v = m.gestureValue + dx * c.Raw("scrub_px");
								t->fade = v < 0.f ? 0.f : v;
							} else if (f == NkGraphField::TransPriority) {
								t->priority = (int32)std::floor(m.gestureValue + dx * c.Raw("scrub_px") * 10.f + 0.5f);
							} else if (m.gestureCondition >= 0 && m.gestureCondition < (int32)t->conditions.Size()) {
								t->conditions[(uint32)m.gestureCondition].threshold = m.gestureValue + dx * c.Raw("scrub_px");
							}
						}
						m.Touch();
						c.r.changed = true;
						break;
					}
					default:
						break;
				}
			}

			void Relache(Ctx &c) {
				NkStateGraphModel &m = c.m;
				if (m.gesture == (uint8)NkStateGraphGesture::Link) {
					const nk_uint64 cible = NoeudSous(c);
					const nk_uint64 source = m.gestureNode;
					if (cible != 0 && cible != source && cible != kNkGraphEntryId && cible != kNkGraphAnyId &&
						cible != kNkGraphUpId) {
						nk_uint64 t = 0;
						if (source == kNkGraphEntryId) {
							m.SetEntry(m.level, cible);
							c.r.changed = true;
						} else if (source == kNkGraphAnyId) {
							t = m.AddAnyTransition(m.level, cible);
						} else {
							t = m.AddTransition(source, cible);
						}
						if (t != 0) {
							Selectionner(c, 0, t);
							c.r.changed = true;
						}
					}
					m.linkMode = false;
				} else if ((m.gesture == (uint8)NkStateGraphGesture::ScrubField) && !m.gestureMoved) {
					// Un CLIC (sans frotter) sur un nombre entier l'avance d'un cran :
					// la moitie droite +1, la gauche -1.
					const NkGraphField f = (NkGraphField)m.gestureParam;
					if (f == NkGraphField::NodeTag || f == NkGraphField::TransPriority) {
						const Ligne *l = LigneSous(c);
						const int32 pas = (l != nullptr && c.in.mouseX < l->hit.rect.x + l->hit.rect.w * 0.5f) ? -1 : 1;
						m.PushUndo();
						if (NkGraphNode *n = m.Node(m.gestureNode)) {
							n->tag = n->tag + pas < 0 ? 0 : n->tag + pas;
						} else if (NkGraphTransition *t = m.Transition(m.gestureNode)) {
							t->priority += pas;
						}
						m.Touch();
						c.r.changed = true;
					}
				}
				m.gesture = (uint8)NkStateGraphGesture::None;
			}

			/// Rend vrai si le double-clic a AGI (l'appui de cette trame ne doit
			/// alors rien faire de plus : les rectangles ont change).
			bool DoubleClic(Ctx &c) {
				NkStateGraphModel &m = c.m;
				// Un nom de parametre : le renommer (l'hote a le clavier).
				for (uint32 i = 0; i < (uint32)c.lignes.Size(); ++i) {
					const Ligne &l = c.lignes[i];
					if ((l.hit.field == NkGraphField::ParamName || l.hit.field == NkGraphField::NodeName) && c.Over(l.hit.rect)) {
						c.r.renameRequested = true;
						c.r.renameRect = l.hit.rect;
						c.r.renameParam = l.hit.field == NkGraphField::ParamName ? l.hit.index : -1;
						c.r.renameNode = l.hit.field == NkGraphField::NodeName ? l.hit.id : 0;
						return true;
					}
				}
				if (!c.Over(c.canvas)) {
					return false;
				}
				const nk_uint64 n = NoeudSous(c);
				m.gesture = (uint8)NkStateGraphGesture::None;
				if (n == kNkGraphUpId) {
					const NkGraphNode *cur = m.Node(m.level);
					ChangerNiveau(c, cur != nullptr ? cur->parent : 0);
					return true;
				}
				if (const NkGraphNode *nn = m.Node(n)) {
					if (nn->subMachine) {
						ChangerNiveau(c, nn->id);
					} else {
						// Double-clic sur un etat : renommer, comme Unity.
						const NkGraphNodeRect *nr = RectDe(c, n);
						c.r.renameRequested = true;
						c.r.renameNode = n;
						c.r.renameRect = nr != nullptr ? nr->rect : NkPaintRect{};
					}
					return true;
				}
				if (n == 0 && ArcSous(c) == 0) {
					float32 gx = 0.f, gy = 0.f;
					NkStateGraphToGraph(m, c.canvas, c.S, c.in.mouseX, c.in.mouseY, gx, gy);
					const nk_uint64 id = m.AddState("Etat", m.level, gx, gy);
					Selectionner(c, id, 0);
					c.r.changed = true;
					return true;
				}
				return false;
			}

			void Molette(Ctx &c) {
				NkStateGraphModel &m = c.m;
				if (c.in.wheel == 0.f || !c.Over(c.canvas)) {
					return;
				}
				float32 gx = 0.f, gy = 0.f;
				NkStateGraphToGraph(m, c.canvas, c.S, c.in.mouseX, c.in.mouseY, gx, gy);
				const float32 f = c.in.wheel > 0.f ? c.Raw("wheel_zoom") : 1.f / c.Raw("wheel_zoom");
				float32 z = m.zoom * f;
				z = z < 0.25f ? 0.25f : (z > 2.5f ? 2.5f : z);
				m.zoom = z;
				// Le point sous la souris reste sous la souris.
				float32 gx2 = 0.f, gy2 = 0.f;
				NkStateGraphToGraph(m, c.canvas, c.S, c.in.mouseX, c.in.mouseY, gx2, gy2);
				m.panX += gx - gx2;
				m.panY += gy - gy2;
			}

			// ── LE DESSIN ───────────────────────────────────────────────────────
			void DessinerFond(Ctx &c) {
				const NkStateGraphModel &m = c.m;
				c.p.Fill(c.canvas, c.s.canvasBg);
				if (NkStateGraphParam(c.s, "show_grid") < 0.5f) {
					return;
				}
				float32 pas = c.M("grid") * m.zoom;
				while (pas < c.M("grid") * 0.5f) {
					pas *= 2.f;
				}
				float32 ox = 0.f, oy = 0.f;
				NkStateGraphToScreen(m, c.canvas, c.S, 0.f, 0.f, ox, oy);
				c.p.PushClip(c.canvas);
				const float32 x0 = c.canvas.x + std::fmod(ox - c.canvas.x, pas) - pas;
				for (float32 x = x0; x < c.canvas.x + c.canvas.w; x += pas) {
					c.p.VLine(x, c.canvas.y, c.canvas.h, c.s.gridLine);
				}
				const float32 y0 = c.canvas.y + std::fmod(oy - c.canvas.y, pas) - pas;
				for (float32 y = y0; y < c.canvas.y + c.canvas.h; y += pas) {
					c.p.HLine(c.canvas.x, y, c.canvas.w, c.s.gridLine);
				}
				c.p.PopClip();
			}

			void DessinerArc(Ctx &c, const Arc &a) {
				NkStateGraphModel &m = c.m;
				const bool choisi = a.id != 0 && a.id == m.selectedTransition;
				const bool tiree = a.id != 0 && a.id == m.firedTransition && m.firedAge < 1.f;
				const bool survol = a.id != 0 && a.id == c.r.hoveredTransition;
				const uint16 role = choisi || tiree ? c.s.accent : (a.entree ? c.s.pseudoEntry : (survol ? c.s.text : c.s.wire));
				const float32 ep = c.M("wire_w") * (choisi || tiree ? 1.6f : 1.f);
				c.p.Line(a.ax, a.ay, a.bx, a.by, role, ep);
				// La pointe AU MILIEU (Unity) : elle reste visible quand les etats se
				// touchent, et dit le sens sans chercher le bout.
				const float32 dx = a.bx - a.ax, dy = a.by - a.ay;
				const float32 l = std::sqrt(dx * dx + dy * dy);
				if (l < 1.f) {
					return;
				}
				const float32 ux = dx / l, uy = dy / l;
				const float32 mx = (a.ax + a.bx) * 0.5f, my = (a.ay + a.by) * 0.5f;
				const float32 t = c.M("arrow") * (m.zoom < 1.f ? 1.f : m.zoom);
				Triangle(c, mx + ux * t, my + uy * t, mx - ux * t * 0.6f - uy * t * 0.7f, my - uy * t * 0.6f + ux * t * 0.7f,
						 mx - ux * t * 0.6f + uy * t * 0.7f, my - uy * t * 0.6f - ux * t * 0.7f, role);
				// Le resume des conditions, a cote de la pointe.
				if (a.id != 0 && m.zoom >= 0.7f && NkStateGraphParam(c.s, "show_conditions") > 0.5f) {
					if (const NkGraphTransition *tr = m.Transition(a.id)) {
						const NkString txt = m.Summary(*tr);
						if (!txt.Empty()) {
							const float32 w = c.p.TextWidth(txt.CStr());
							const float32 nx = -uy, ny = ux;
							const float32 lx = mx + nx * t * 2.f - w * 0.5f;
							const float32 ly = my + ny * t * 2.f - c.M("row_h") * 0.5f;
							c.p.FillColor(NkPaintRect{lx - c.M("button_gap"), ly, w + c.M("button_gap") * 2.f, c.M("row_h")},
										  Voile(c.p, c.s.canvasBg, 0xC0u), c.M("button_gap"));
							c.p.Text(NkPaintRect{lx, ly, w + c.M("stroke_w"), c.M("row_h")}, txt.CStr(), choisi ? c.s.text : c.s.textMuted,
									 NkTextAlign::Left);
						}
					}
				}
			}

			void DessinerNoeud(Ctx &c, const NkGraphNodeRect &nr) {
				NkStateGraphModel &m = c.m;
				const NkPaintRect &rc = nr.rect;
				const float32 rnd = c.M("node_round") * m.zoom;
				const bool choisi = nr.id == m.selectedNode;
				const bool survol = nr.id == c.r.hoveredNode;
				if (nr.id == kNkGraphEntryId || nr.id == kNkGraphAnyId || nr.id == kNkGraphUpId) {
					const uint16 role = nr.id == kNkGraphEntryId ? c.s.pseudoEntry : (nr.id == kNkGraphAnyId ? c.s.pseudoAny : c.s.headerBg);
					if (choisi) {
						c.p.Fill(NkPaintRect{rc.x - c.M("wire_w"), rc.y - c.M("wire_w"), rc.w + c.M("wire_w") * 2.f,
											 rc.h + c.M("wire_w") * 2.f},
								 c.s.accent, rc.h * 0.5f);
					}
					c.p.Fill(rc, role, rc.h * 0.5f);
					NkString nom = nr.id == kNkGraphEntryId ? NkString("Entree")
															: (nr.id == kNkGraphAnyId ? NkString("N'importe quel etat") : NkString("^ "));
					if (nr.id == kNkGraphUpId) {
						const NkGraphNode *cur = m.Node(m.level);
						const NkGraphNode *par = cur != nullptr ? m.Node(cur->parent) : nullptr;
						nom.Append(par != nullptr ? par->name : NkString("Racine"));
					}
					c.p.Text(rc, nom.CStr(), nr.id == kNkGraphUpId ? c.s.text : c.s.textOnAccent, NkTextAlign::Center);
				} else {
					const NkGraphNode *n = m.Node(nr.id);
					if (n == nullptr) {
						return;
					}
					const bool entree = m.EntryOf(n->parent) == n->id;
					// L'apercu : l'etat ACTIF (ou celui qui le contient) a une bordure
					// ACCENT epaisse (spec. 06 §7) ; celui vers lequel on fond, une fine.
					const bool actif = m.live && m.liveState != 0 && m.RepresentativeAt(m.liveState, m.level) == n->id;
					const bool suivant = m.live && m.liveNext != 0 && m.RepresentativeAt(m.liveNext, m.level) == n->id;
					const float32 bord = c.M("wire_w") * (actif ? 2.f : 1.f);
					if (actif || suivant || choisi) {
						c.p.Fill(NkPaintRect{rc.x - bord, rc.y - bord, rc.w + bord * 2.f, rc.h + bord * 2.f},
								 actif || choisi ? c.s.accent : c.s.text, rnd + bord);
					}
					if (n->subMachine) {
						// Une PILE : un second cadre decale dit « il y a des etats dedans ».
						const float32 d = c.M("button_gap") * 2.f * m.zoom;
						c.p.Fill(NkPaintRect{rc.x + d, rc.y + d, rc.w, rc.h}, c.s.border, rnd);
					}
					c.p.Fill(rc, c.s.nodeBody, rnd);
					const uint16 bandeau = n->subMachine ? c.s.subMachine : (entree ? c.s.nodeEntry : c.s.nodeHeader);
					c.p.Fill(NkPaintRect{rc.x, rc.y, rc.w, c.M("node_header_h") * m.zoom}, bandeau, rnd);
					const float32 pad = c.M("pad") * m.zoom;
					const float32 lh = rc.h - c.M("node_header_h") * m.zoom;
					c.p.Text(NkPaintRect{rc.x + pad, rc.y + c.M("node_header_h") * m.zoom, rc.w - pad * 2.f, lh * 0.55f},
							 n->name.CStr(), c.s.text, NkTextAlign::Center);
					if (m.zoom >= 0.6f) {
						const char *sous = n->subMachine ? "sous-machine" : (n->clip.Empty() ? "(etat vide)" : n->clip.CStr());
						c.p.Text(NkPaintRect{rc.x + pad, rc.y + rc.h - lh * 0.48f, rc.w - pad * 2.f, lh * 0.42f}, sous,
								 c.s.textMuted, NkTextAlign::Center);
					}
					if (actif && m.liveFade > 0.f) {
						// Le FONDU en cours : une barre qui se remplit sous l'etat.
						c.p.Fill(NkPaintRect{rc.x, rc.y + rc.h - c.M("button_gap"), rc.w * (1.f - m.liveFade), c.M("button_gap")},
								 c.s.accent);
					}
				}
				// Le PORT : d'ou part une transition (au survol, ou si l'etat est choisi).
				if (nr.id != kNkGraphUpId && (survol || choisi || m.linkMode)) {
					c.p.Ellipse(nr.port, c.s.border);
					const float32 r2 = nr.port.w * 0.32f;
					c.p.Ellipse(NkPaintRect{nr.port.x + nr.port.w * 0.5f - r2, nr.port.y + nr.port.h * 0.5f - r2, r2 * 2.f, r2 * 2.f},
								c.s.accent);
				}
			}

			void DessinerLien(Ctx &c) {
				const NkStateGraphModel &m = c.m;
				if (m.gesture != (uint8)NkStateGraphGesture::Link) {
					return;
				}
				const NkGraphNodeRect *nr = RectDe(c, m.gestureNode);
				if (nr == nullptr) {
					return;
				}
				const float32 x0 = nr->port.x + nr->port.w * 0.5f, y0 = nr->port.y + nr->port.h * 0.5f;
				c.p.Line(x0, y0, c.in.mouseX, c.in.mouseY, c.s.accent, c.M("wire_w"));
			}

			void DessinerLigne(Ctx &c, const Ligne &l, uint16 fond) {
				const NkPaintRect &rc = l.hit.rect;
				const float32 pad = c.M("pad");
				const float32 gap = c.M("button_gap");
				if (l.titre) {
					c.p.Fill(rc, c.s.headerBg);
					c.p.Text(NkPaintRect{rc.x + pad, rc.y, rc.w - pad * 2.f, rc.h}, l.texte.CStr(), c.s.text, NkTextAlign::Left);
					return;
				}
				const bool survol = l.actif && l.hit.field != NkGraphField::None && c.Over(rc);
				if (l.hit.field == NkGraphField::Breadcrumb) {
					c.p.Text(rc, l.texte.CStr(), l.actif ? (survol ? c.s.accent : c.s.textMuted) : c.s.text, NkTextAlign::Left);
					return;
				}
				if (l.bouton) {
					c.p.Fill(rc, survol ? c.s.inputBg : c.s.buttonBg, gap);
					const char *t = !l.texte.Empty() ? l.texte.CStr() : l.valeur.CStr();
					c.p.Text(rc, t, l.actif ? c.s.text : c.s.textMuted, NkTextAlign::Center);
					return;
				}
				if (l.hit.field == NkGraphField::ParamRemove) {
					if (c.Over(NkPaintRect{c.side.x, rc.y, c.side.w, rc.h})) {
						const float32 cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f, d = rc.h * 0.2f;
						c.p.Line(cx - d, cy, cx + d, cy, survol ? c.s.text : c.s.textMuted, c.M("stroke_w") * 2.f);
					}
					return;
				}
				if (l.hit.field == NkGraphField::ParamName || l.hit.field == NkGraphField::ClipItem ||
					(l.hit.field == NkGraphField::None && !l.texte.Empty() && l.valeur.Empty())) {
					float32 x = rc.x;
					if (l.pastille != 0) {
						const float32 d = rc.h * 0.36f;
						c.p.Fill(NkPaintRect{x, rc.y + (rc.h - d) * 0.5f, d, d}, l.pastille, d * 0.5f);
						x += d + gap * 2.f;
					}
					if (survol && l.hit.field == NkGraphField::ClipItem) {
						c.p.Fill(rc, c.s.inputBg, gap);
					}
					const float32 vw = l.hit.field == NkGraphField::ClipItem ? c.p.TextWidth(l.valeur.CStr()) + pad : 0.f;
					c.p.Text(NkPaintRect{x, rc.y, rc.x + rc.w - x - vw, rc.h}, l.texte.CStr(), l.actif ? c.s.text : c.s.textMuted,
							 NkTextAlign::Left);
					if (vw > 0.f && survol) {
						c.p.Text(NkPaintRect{rc.x + rc.w - vw, rc.y, vw, rc.h}, l.valeur.CStr(), c.s.accent, NkTextAlign::Right);
					}
					return;
				}
				// Un champ : le libelle a gauche (dans l'inspecteur), la valeur dans sa case.
				if (!l.texte.Empty() && rc.x > c.inspector.x) {
					const NkPaintRect lib{c.inspector.x + pad, rc.y, rc.x - c.inspector.x - pad * 2.f, rc.h};
					c.p.Text(lib, l.texte.CStr(), c.s.textMuted, NkTextAlign::Left);
				}
				c.p.Fill(rc, survol ? c.s.buttonBg : fond, gap);
				if (l.hit.field == NkGraphField::ParamValue && !l.bouton && l.valeur == NkString(l.coche ? "vrai" : "faux")) {
					// Un booleen : une CASE.
					const float32 d = rc.h - gap * 2.f;
					const NkPaintRect cs{rc.x + rc.w - d - gap, rc.y + gap, d, d};
					c.p.Fill(cs, l.coche ? c.s.accent : c.s.border, gap);
					c.p.Text(NkPaintRect{rc.x + gap * 2.f, rc.y, rc.w - d - gap * 4.f, rc.h}, l.valeur.CStr(), c.s.text,
							 NkTextAlign::Left);
					return;
				}
				c.p.Text(NkPaintRect{rc.x + gap * 2.f, rc.y, rc.w - gap * 4.f, rc.h}, l.valeur.CStr(), c.s.text,
						 l.hit.field == NkGraphField::CondOp ? NkTextAlign::Center : NkTextAlign::Left);
			}

			void DessinerPanneaux(Ctx &c) {
				c.p.Fill(c.side, c.s.panelBg);
				c.p.Fill(c.inspector, c.s.panelBg);
				c.p.Fill(c.bar, c.s.headerBg);
				c.p.VLine(c.side.x + c.side.w, c.side.y, c.side.h, c.s.border);
				c.p.VLine(c.inspector.x - c.M("stroke_w"), c.inspector.y, c.inspector.h, c.s.border);
				c.p.HLine(c.bar.x, c.bar.y + c.bar.h - c.M("stroke_w"), c.bar.w, c.s.border);
				static const char *const kTextes[] = {"+ Etat", "+ Sous-machine", "Relier", "Supprimer", "Annuler",
													  "Refaire", "Cadrer", "+ Bool", "+ Reel", "+ Decl."};
				for (uint8 b = 0; b < (uint8)NkStateGraphButton::Count; ++b) {
					const NkPaintRect &rc = c.r.buttons[b];
					if (rc.w <= 0.f) {
						continue;
					}
					bool actif = true;
					if (b == (uint8)NkStateGraphButton::Undo) {
						actif = c.m.CanUndo();
					} else if (b == (uint8)NkStateGraphButton::Redo) {
						actif = c.m.CanRedo();
					} else if (b == (uint8)NkStateGraphButton::Delete) {
						actif = c.m.selectedTransition != 0 || c.m.Node(c.m.selectedNode) != nullptr;
					}
					const bool enfonce = b == (uint8)NkStateGraphButton::Link && c.m.linkMode;
					c.p.Fill(rc, enfonce ? c.s.accent : (c.Over(rc) ? c.s.inputBg : c.s.buttonBg), c.M("button_gap"));
					c.p.Text(rc, kTextes[b], enfonce ? c.s.textOnAccent : (actif ? c.s.text : c.s.textMuted), NkTextAlign::Center);
				}
				// Le fil d'Ariane : « Racine > Sol ».
				for (uint32 i = 0; i < (uint32)c.lignes.Size(); ++i) {
					const Ligne &l = c.lignes[i];
					if (l.hit.field == NkGraphField::Breadcrumb && l.hit.index > 0) {
						c.p.Text(NkPaintRect{l.hit.rect.x - c.p.TextWidth(" > "), l.hit.rect.y, c.p.TextWidth(" > "), l.hit.rect.h}, " > ",
								 c.s.textMuted, NkTextAlign::Center);
					}
				}
				for (uint32 i = 0; i < (uint32)c.lignes.Size(); ++i) {
					DessinerLigne(c, c.lignes[i], c.s.inputBg);
				}
				if (c.m.live) {
					// Le bandeau de l'apercu : l'etat courant, en clair.
					NkString t("Apercu : ");
					t.Append(c.m.liveState != 0 ? c.m.PathOf(c.m.liveState) : NkString("(rien)"));
					const float32 w = c.p.TextWidth(t.CStr()) + c.M("pad") * 2.f;
					const NkPaintRect b{c.canvas.x + c.M("pad"), c.canvas.y + c.M("pad"), w, c.M("row_h")};
					c.p.Fill(b, c.s.accent, c.M("button_gap"));
					c.p.Text(b, t.CStr(), c.s.textOnAccent, NkTextAlign::Center);
				}
			}

		} // namespace

		NkStateGraphResult NkDrawStateGraph(NkComponentPaint &p, const NkComponentInput &in, const NkPaintRect &rect,
											NkStateGraphModel &m, const NkStateGraphStyle &s) {
			NkStateGraphResult r;
			Ctx c{p, in, m, s, r};
			c.S = in.surfaceScale > 0.f ? in.surfaceScale : 1.f;
			if (rect.w <= 0.f || rect.h <= 0.f) {
				return r;
			}
			Planifier(c, rect);
			m.PlacePseudos(m.level);
			Construire(c);
			// Les GESTES, avant le dessin.
			const bool double_ = in.doubleClick && DoubleClic(c);
			if (in.mousePressed && m.gesture == (uint8)NkStateGraphGesture::None && !double_) {
				Appui(c);
			} else if (in.mouseDown && m.gesture != (uint8)NkStateGraphGesture::None) {
				Tenu(c);
			}
			if (in.mouseReleased && m.gesture != (uint8)NkStateGraphGesture::None) {
				Tenu(c);
				Relache(c);
			}
			Molette(c);
			// La geometrie RECALCULEE (un etat cree, un niveau change...).
			m.PlacePseudos(m.level);
			Construire(c);
			r.hoveredNode = c.Over(c.canvas) ? NoeudSous(c) : 0;
			r.hoveredTransition = (c.Over(c.canvas) && r.hoveredNode == 0) ? ArcSous(c) : 0;
			for (uint32 i = 0; i < (uint32)c.lignes.Size(); ++i) {
				if (c.lignes[i].hit.field != NkGraphField::None) {
					r.hits.PushBack(c.lignes[i].hit);
				}
			}
			// Le dessin.
			DessinerFond(c);
			c.p.PushClip(c.canvas);
			for (uint32 i = 0; i < (uint32)c.arcs.Size(); ++i) {
				DessinerArc(c, c.arcs[i]);
			}
			for (uint32 i = 0; i < (uint32)r.nodeRects.Size(); ++i) {
				DessinerNoeud(c, r.nodeRects[i]);
			}
			DessinerLien(c);
			c.p.PopClip();
			DessinerPanneaux(c);
			return r;
		}

	} // namespace editorkit
} // namespace nkentseu
