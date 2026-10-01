// -----------------------------------------------------------------------------
// @File    NkTimelineDraw.cpp
// @Brief   Le dessin et les gestes de la frise (feuille d'images et courbes).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA REGLE (celle de NkTabStripDraw.cpp et NkTreeViewDraw.cpp)
// =============================================================================
//  ⚠️ AUCUN NOMBRE DE PIXELS ICI : toute longueur passe par `M("...")`, toute
//     couleur par un role du style. Les seules constantes tolerees sont des
//     fractions sans dimension (0.5f pour centrer) et des alphas de voile.
//
//  L'ORDRE D'UNE TRAME :
//   1. `Plan`  — la geometrie (barre, colonne, regle, zone, lignes), UNE fois :
//      le dessin et les gestes la lisent tous les deux.
//   2. les gestes qui COMMENCENT (appui), CONTINUENT (bouton tenu), FINISSENT
//      (relache) -- ils modifient le modele AVANT le dessin, qui montre donc
//      deja l'etat de cette trame ;
//   3. le dessin.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkTimelineModel.h"

#include <cmath>
#include <cstdio>

namespace nkentseu {
	namespace editorkit {

		namespace {

			float32 Abs(float32 v) {
				return v < 0.f ? -v : v;
			}

			/// Les libelles COURTS des cases d'interpolation (la barre doit tenir dans
			/// une fenetre de 1280 px ; le nom long reste NkTimelineInterpName).
			const char *Court(uint8 i) {
				static const char *const k[(uint8)NkTimelineInterp::Count] = {"Palier", "Lin.",	  "Courbe", "Entree", "Sortie",
																			  "E/S",	"Rebond", "Elast.", "Recul"};
				return i < (uint8)NkTimelineInterp::Count ? k[i] : "?";
			}

			struct Plan {
					NkPaintRect toolbar, list, listHead, ruler, area;
					float32 rowH = 0.f, groupH = 0.f;
			};

			struct Ctx {
					NkComponentPaint &p;
					const NkComponentInput &in;
					NkTimelineModel &m;
					const NkTimelineStyle &s;
					const NkTimelineHooks &h;
					NkTimelineResult &r;
					float32 S = 1.f;
					Plan plan;

					float32 M(const char *n) const {
						return NkTimelineMetric(s, n) * S;
					}
					float32 Raw(const char *n) const {
						return NkTimelineMetric(s, n);
					}
					bool Over(const NkPaintRect &rc) const {
						return rc.w > 0.f && rc.Contains(in.mouseX, in.mouseY);
					}
					bool Eval(const NkTimelineTrack &tr, float32 t, float32 out[4]) const {
						out[0] = out[1] = out[2] = out[3] = 0.f;
						if (h.evaluate != nullptr && !tr.keys.Empty() && h.evaluate(h.user, tr, t, out)) {
							return true;
						}
						return m.EvaluateFallback(tr, t, out);
					}
					/// La valeur montree au curseur : celle de la courbe, ou la valeur
					/// vivante de l'objet si la piste n'a pas encore de cle.
					void ValueAtCursor(const NkTimelineTrack &tr, float32 out[4]) const {
						if (!Eval(tr, m.cursor, out)) {
							if (h.readLive == nullptr || !h.readLive(h.user, tr, out)) {
								out[0] = out[1] = out[2] = out[3] = 0.f;
							}
						}
					}
					/// La valeur a poser par « ◆ » : la vivante d'abord (ce que l'objet
					/// montre), sinon celle de la courbe.
					void ValueToKey(const NkTimelineTrack &tr, float32 t, float32 out[4]) const {
						if (h.readLive == nullptr || !h.readLive(h.user, tr, out)) {
							if (!Eval(tr, t, out)) {
								out[0] = out[1] = out[2] = out[3] = 0.f;
							}
						}
					}
			};

			uint32 Voile(NkComponentPaint &p, uint16 role, uint32 alpha) {
				return (p.ColorOf(role) & 0xFFFFFF00u) | (alpha & 0xFFu);
			}

			void Losange(Ctx &c, float32 cx, float32 cy, float32 r, uint16 role) {
				const float32 xy[8] = {cx, cy - r, cx + r, cy, cx, cy + r, cx - r, cy};
				if (!c.p.PolygonHex(xy, 4, c.p.ColorOf(role))) {
					c.p.Fill(NkPaintRect{cx - r * 0.7f, cy - r * 0.7f, r * 1.4f, r * 1.4f}, role);
				}
			}

			/// Un triangle plein (icones de transport), sommets en paires.
			void Triangle(Ctx &c, float32 x0, float32 y0, float32 x1, float32 y1, float32 x2, float32 y2, uint16 role) {
				const float32 xy[6] = {x0, y0, x1, y1, x2, y2};
				if (!c.p.PolygonHex(xy, 3, c.p.ColorOf(role))) {
					c.p.Line(x0, y0, x1, y1, role, c.M("stroke_w"));
					c.p.Line(x1, y1, x2, y2, role, c.M("stroke_w"));
					c.p.Line(x2, y2, x0, y0, role, c.M("stroke_w"));
				}
			}

			const char *NomObjet(const NkString &o) {
				return o.Empty() ? "(objet anime)" : o.CStr();
			}

			const char *NomCanal(const NkTimelineTrack &tr, uint32 c) {
				if (c < 4 && !tr.channelNames[c].Empty()) {
					return tr.channelNames[c].CStr();
				}
				static const char *const kXyzw[4] = {"X", "Y", "Z", "W"};
				static const char *const kRgba[4] = {"R", "G", "B", "A"};
				return tr.kind == NkTimelineValueKind::Couleur ? kRgba[c & 3u] : kXyzw[c & 3u];
			}

			// ── 1. LE PLAN ──────────────────────────────────────────────────────
			void Planifier(Ctx &c, const NkPaintRect &rect) {
				Plan &pl = c.plan;
				const float32 tb = c.M("toolbar_h");
				const float32 rh = c.M("ruler_h");
				float32 lw = c.M("list_w");
				if (lw > rect.w * 0.5f) {
					lw = rect.w * 0.5f;
				}
				pl.toolbar = NkPaintRect{rect.x, rect.y, rect.w, tb};
				const float32 corpsY = rect.y + tb;
				const float32 corpsH = rect.h - tb > 0.f ? rect.h - tb : 0.f;
				pl.listHead = NkPaintRect{rect.x, corpsY, lw, rh};
				pl.list = NkPaintRect{rect.x, corpsY + rh, lw, corpsH - rh > 0.f ? corpsH - rh : 0.f};
				const float32 ax = rect.x + lw + c.M("stroke_w");
				const float32 aw = rect.x + rect.w - ax > 0.f ? rect.x + rect.w - ax : 0.f;
				pl.ruler = NkPaintRect{ax, corpsY, aw, rh};
				pl.area = NkPaintRect{ax, corpsY + rh, aw, pl.list.h};
				pl.rowH = c.M("row_h");
				pl.groupH = c.M("group_h");
				c.r.list = pl.list;
				c.r.ruler = pl.ruler;
				c.r.area = pl.area;
			}

			/// Les lignes visibles : un en-tete par objet, puis ses pistes (sauf
			/// objet replie). `rowScroll` saute les premieres.
			void Lignes(Ctx &c) {
				NkTimelineModel &m = c.m;
				NkVector<NkTimelineRow> toutes;
				for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
					const NkTimelineTrack &tr = m.tracks[i];
					if (i == 0 || !(m.tracks[i - 1].object == tr.object)) {
						NkTimelineRow g;
						g.track = 0;
						g.object = tr.object;
						g.h = c.plan.groupH;
						toutes.PushBack(g);
					}
					if (!m.IsCollapsed(tr.object)) {
						NkTimelineRow l;
						l.track = tr.id;
						l.object = tr.object;
						l.h = c.plan.rowH;
						toutes.PushBack(l);
					}
				}
				if (m.rowScroll > (int32)toutes.Size() - 1) {
					m.rowScroll = (int32)toutes.Size() - 1;
				}
				if (m.rowScroll < 0) {
					m.rowScroll = 0;
				}
				float32 y = c.plan.list.y;
				for (uint32 k = (uint32)m.rowScroll; k < (uint32)toutes.Size(); ++k) {
					if (y >= c.plan.list.y + c.plan.list.h) {
						break;
					}
					NkTimelineRow l = toutes[k];
					l.y = y;
					c.r.rows.PushBack(l);
					y += l.h;
				}
			}

			const NkTimelineRow *LigneSous(const Ctx &c, float32 y) {
				for (uint32 k = 0; k < (uint32)c.r.rows.Size(); ++k) {
					const NkTimelineRow &l = c.r.rows[k];
					if (y >= l.y && y < l.y + l.h) {
						return &l;
					}
				}
				return nullptr;
			}

			// ── LA BARRE D'OUTILS : la geometrie ────────────────────────────────
			void PlacerBoutons(Ctx &c) {
				const Plan &pl = c.plan;
				const float32 bw = c.M("button_w");
				const float32 gap = c.M("button_gap");
				const float32 grp = c.M("group_gap");
				const float32 pad = c.M("pad");
				const float32 by = pl.toolbar.y + c.M("button_gap") * 2.f;
				const float32 bh = pl.toolbar.h - c.M("button_gap") * 4.f;
				float32 x = pl.toolbar.x + pad;
				const float32 fin = pl.toolbar.x + pl.toolbar.w - pad;
				auto poser = [&](NkTimelineButton b, float32 w) {
					NkPaintRect rc{x, by, w, bh};
					if (x + w > fin) {
						rc.w = 0.f; // hors de la barre : ni dessine, ni cliquable
					}
					c.r.buttons[(uint8)b] = rc;
					x += w + gap;
				};
				auto texte = [&](const char *t) { return c.p.TextWidth(t) + pad * 2.f; };
				poser(NkTimelineButton::First, bw);
				poser(NkTimelineButton::PrevKey, bw);
				poser(NkTimelineButton::Play, bw);
				poser(NkTimelineButton::NextKey, bw);
				poser(NkTimelineButton::Last, bw);
				poser(NkTimelineButton::Loop, bw);
				x += c.p.TextWidth("0000 / 0000") + pad * 2.f + grp; // le compteur d'images
				poser(NkTimelineButton::Mode, texte("Courbes"));
				poser(NkTimelineButton::Snap, texte("Accrocher"));
				poser(NkTimelineButton::FrameAll, texte("Cadrer"));
				x += grp;
				poser(NkTimelineButton::AddKey, texte("+ Cle"));
				poser(NkTimelineButton::Delete, texte("Suppr."));
				poser(NkTimelineButton::Undo, texte("Annuler"));
				poser(NkTimelineButton::Redo, texte("Refaire"));
				x += grp;
				for (uint8 i = 0; i < (uint8)NkTimelineInterp::Count; ++i) {
					poser((NkTimelineButton)((uint8)NkTimelineButton::InterpFirst + i), texte(Court(i)));
				}
				// « + Propriete » vit en tete de la colonne, au-dessus des pistes.
				const float32 aw = c.p.TextWidth("+ Propriete") + pad * 2.f;
				c.r.buttons[(uint8)NkTimelineButton::AddTrack] =
					NkPaintRect{pl.listHead.x + pad, pl.listHead.y + gap * 2.f, aw, pl.listHead.h - gap * 4.f};
			}

			// ── 2. LES GESTES ───────────────────────────────────────────────────
			/// La cle sous le curseur dans la zone (feuille d'images), -1 sinon.
			int32 CleSous(Ctx &c, const NkTimelineRow &l, nk_uint64 &piste) {
				const float32 hit = c.M("hit_r");
				const float32 cy = l.y + l.h * 0.5f;
				if (Abs(c.in.mouseY - cy) > hit) {
					return -1;
				}
				int32 meilleur = -1;
				float32 d = hit;
				for (uint32 i = 0; i < (uint32)c.m.tracks.Size(); ++i) {
					const NkTimelineTrack &tr = c.m.tracks[i];
					if (l.track != 0 ? tr.id != l.track : !(tr.object == l.object)) {
						continue;
					}
					for (uint32 k = 0; k < (uint32)tr.keys.Size(); ++k) {
						const float32 kx = NkTimelineTimeToX(c.m, c.plan.area, tr.keys[k].time);
						if (Abs(kx - c.in.mouseX) <= d) {
							d = Abs(kx - c.in.mouseX);
							meilleur = (int32)k;
							piste = tr.id;
						}
					}
				}
				return meilleur;
			}

			/// Les pistes MONTREES dans l'editeur de courbes : la piste active, ou
			/// toutes celles qui le demandent.
			bool CourbeMontree(const NkTimelineModel &m, const NkTimelineTrack &tr) {
				return m.activeTrack != 0 ? tr.id == m.activeTrack : tr.showCurve;
			}

			/// Le point de courbe sous le curseur (piste, cle, canal).
			bool PointSous(Ctx &c, nk_uint64 &piste, int32 &cle, int32 &canal) {
				const float32 hit = c.M("hit_r");
				float32 meilleur = hit * hit;
				bool trouve = false;
				for (uint32 i = 0; i < (uint32)c.m.tracks.Size(); ++i) {
					const NkTimelineTrack &tr = c.m.tracks[i];
					if (!CourbeMontree(c.m, tr)) {
						continue;
					}
					for (uint32 k = 0; k < (uint32)tr.keys.Size(); ++k) {
						const float32 kx = NkTimelineTimeToX(c.m, c.plan.area, tr.keys[k].time);
						for (uint32 ch = 0; ch < tr.channels; ++ch) {
							if (!tr.showChannel[ch]) {
								continue;
							}
							const float32 ky = NkTimelineValueToY(c.m, c.plan.area, tr.keys[k].v[ch]);
							const float32 dx = kx - c.in.mouseX, dy = ky - c.in.mouseY;
							if (dx * dx + dy * dy <= meilleur) {
								meilleur = dx * dx + dy * dy;
								piste = tr.id;
								cle = (int32)k;
								canal = (int32)ch;
								trouve = true;
							}
						}
					}
				}
				return trouve;
			}

			/// Memorise, dans l'ordre de parcours, le temps (et la valeur du canal
			/// tenu) de chaque cle choisie : le glisser repart toujours de la.
			void RetenirChoisies(Ctx &c) {
				NkTimelineModel &m = c.m;
				m.gestureTimes.Clear();
				m.gestureValues.Clear();
				for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
					for (uint32 k = 0; k < (uint32)m.tracks[i].keys.Size(); ++k) {
						const NkTimelineKey &key = m.tracks[i].keys[k];
						if (key.selected) {
							m.gestureTimes.PushBack(key.time);
							const int32 ch = m.gestureChannel;
							m.gestureValues.PushBack(ch >= 0 && ch < 4 ? key.v[ch] : 0.f);
						}
					}
				}
			}

			void Glisser(Ctx &c) {
				NkTimelineModel &m = c.m;
				const NkPaintRect &area = c.plan.area;
				const float32 dt = NkTimelineXToTime(m, area, c.in.mouseX) - m.gestureT0;
				const float32 dv = m.curveMode && m.gestureChannel >= 0
									   ? NkTimelineYToValue(m, area, c.in.mouseY) - m.gestureV0
									   : 0.f;
				uint32 n = 0;
				for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
					NkTimelineTrack &tr = m.tracks[i];
					for (uint32 k = 0; k < (uint32)tr.keys.Size(); ++k) {
						NkTimelineKey &key = tr.keys[k];
						if (!key.selected || n >= (uint32)m.gestureTimes.Size()) {
							continue;
						}
						float32 t = m.Snap(m.gestureTimes[n] + dt);
						key.time = t < 0.f ? 0.f : t;
						if (m.curveMode && tr.id == m.gestureTrack && m.gestureChannel >= 0 &&
							(uint32)m.gestureChannel < tr.channels) {
							key.v[m.gestureChannel] = m.gestureValues[n] + dv;
						}
						++n;
					}
				}
				m.Touch();
				c.r.changed = true;
			}

			void BoiteChoisir(Ctx &c, bool additive) {
				NkTimelineModel &m = c.m;
				const NkPaintRect &area = c.plan.area;
				const float32 x0 = m.gestureX0 < c.in.mouseX ? m.gestureX0 : c.in.mouseX;
				const float32 x1 = m.gestureX0 < c.in.mouseX ? c.in.mouseX : m.gestureX0;
				const float32 y0 = m.gestureY0 < c.in.mouseY ? m.gestureY0 : c.in.mouseY;
				const float32 y1 = m.gestureY0 < c.in.mouseY ? c.in.mouseY : m.gestureY0;
				if (!additive) {
					m.ClearSelection();
				}
				const float32 t0 = NkTimelineXToTime(m, area, x0);
				const float32 t1 = NkTimelineXToTime(m, area, x1);
				if (m.curveMode) {
					for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
						NkTimelineTrack &tr = m.tracks[i];
						if (!CourbeMontree(m, tr)) {
							continue;
						}
						for (uint32 k = 0; k < (uint32)tr.keys.Size(); ++k) {
							NkTimelineKey &key = tr.keys[k];
							if (key.time < t0 || key.time > t1) {
								continue;
							}
							for (uint32 ch = 0; ch < tr.channels; ++ch) {
								const float32 ky = NkTimelineValueToY(m, area, key.v[ch]);
								if (tr.showChannel[ch] && ky >= y0 && ky <= y1) {
									key.selected = true;
								}
							}
						}
					}
					return;
				}
				for (uint32 k = 0; k < (uint32)c.r.rows.Size(); ++k) {
					const NkTimelineRow &l = c.r.rows[k];
					const float32 cy = l.y + l.h * 0.5f;
					if (cy < y0 || cy > y1) {
						continue;
					}
					for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
						const bool dedans = l.track != 0 ? m.tracks[i].id == l.track : m.tracks[i].object == l.object;
						if (dedans) {
							m.SelectInBox((int32)i, (int32)i, t0, t1, true);
						}
					}
				}
			}

			/// Les sous-zones d'une ligne de piste dans la colonne : « ◆ », « − »,
			/// et une case par canal.
			struct ZonesLigne {
					NkPaintRect cle, retirer, canal[4], bascule[4];
					NkPaintRect libelle;
			};

			ZonesLigne Zones(const Ctx &c, const NkTimelineRow &l, const NkTimelineTrack &tr) {
				ZonesLigne z;
				const NkPaintRect &li = c.plan.list;
				const float32 pad = c.M("pad");
				const float32 bw = l.h;
				float32 x = li.x + li.w - pad - bw;
				z.cle = NkPaintRect{x, l.y, bw, l.h};
				x -= bw;
				z.retirer = NkPaintRect{x, l.y, bw, l.h};
				const bool valeurs = NkTimelineParam(c.s, "show_values") > 0.5f;
				float32 vw = c.M("value_w");
				const float32 place = li.w * 0.6f;
				if (vw * (float32)tr.channels > place) {
					vw = place / (float32)tr.channels;
				}
				for (int32 ch = (int32)tr.channels - 1; ch >= 0 && valeurs; --ch) {
					x -= vw;
					z.canal[ch] = NkPaintRect{x, l.y + c.M("button_gap"), vw - c.M("button_gap"), l.h - c.M("button_gap") * 2.f};
					z.bascule[ch] = NkPaintRect{x, l.y + c.M("button_gap"), c.M("button_gap") * 2.f, l.h - c.M("button_gap") * 2.f};
				}
				const float32 lx = li.x + pad + c.M("indent");
				z.libelle = NkPaintRect{lx, l.y, x - pad - lx > 0.f ? x - pad - lx : 0.f, l.h};
				return z;
			}

			void ActionBouton(Ctx &c, uint8 b) {
				NkTimelineModel &m = c.m;
				switch ((NkTimelineButton)b) {
					case NkTimelineButton::First:
						m.SetCursor(0.f);
						c.r.cursorChanged = true;
						break;
					case NkTimelineButton::PrevKey:
						m.SetCursor(m.PreviousKeyTime(m.cursor));
						c.r.cursorChanged = true;
						break;
					case NkTimelineButton::Play:
						m.TogglePlay();
						break;
					case NkTimelineButton::NextKey:
						m.SetCursor(m.NextKeyTime(m.cursor));
						c.r.cursorChanged = true;
						break;
					case NkTimelineButton::Last:
						m.SetCursor(m.duration);
						c.r.cursorChanged = true;
						break;
					case NkTimelineButton::Loop:
						m.loop = !m.loop;
						break;
					case NkTimelineButton::Mode:
						m.curveMode = !m.curveMode;
						m.curveAutoFit = true;
						break;
					case NkTimelineButton::Snap:
						m.snap = !m.snap;
						break;
					case NkTimelineButton::FrameAll:
						m.FrameAll();
						break;
					case NkTimelineButton::AddKey: {
						const NkTimelineTrack *tr = m.Track(m.activeTrack);
						if (tr == nullptr && !m.tracks.Empty()) {
							tr = &m.tracks[0];
						}
						if (tr != nullptr) {
							float32 v[4];
							c.ValueToKey(*tr, m.cursor, v);
							m.SetKey(tr->id, m.cursor, v);
							c.r.changed = true;
						}
						break;
					}
					case NkTimelineButton::Delete:
						c.r.changed = m.DeleteSelection() || c.r.changed;
						break;
					case NkTimelineButton::Undo:
						c.r.undone = m.Undo();
						c.r.changed = c.r.changed || c.r.undone;
						break;
					case NkTimelineButton::Redo:
						c.r.redone = m.Redo();
						c.r.changed = c.r.changed || c.r.redone;
						break;
					case NkTimelineButton::AddTrack:
						c.r.addTrackRequested = true;
						break;
					default:
						if (b >= (uint8)NkTimelineButton::InterpFirst && b < (uint8)NkTimelineButton::Count) {
							const uint32 avant = m.revision;
							m.SetSelectionInterp((uint8)(b - (uint8)NkTimelineButton::InterpFirst));
							c.r.changed = c.r.changed || m.revision != avant;
						}
						break;
				}
			}

			void Appui(Ctx &c) {
				NkTimelineModel &m = c.m;
				const NkComponentInput &in = c.in;
				const bool ajout = in.ctrl || in.shift;
				m.gestureX0 = in.mouseX;
				m.gestureY0 = in.mouseY;
				m.gestureMoved = false;
				m.gestureSnapshot = false;
				m.gestureChannel = -1;
				// La barre d'outils et « + Propriete ».
				for (uint8 b = 0; b < (uint8)NkTimelineButton::Count; ++b) {
					if (c.Over(c.r.buttons[b])) {
						ActionBouton(c, b);
						return;
					}
				}
				const Plan &pl = c.plan;
				if (c.Over(pl.ruler)) {
					m.gesture = (uint8)NkTimelineGesture::Scrub;
					m.playing = false;
					m.SetCursor(m.Snap(NkTimelineXToTime(m, pl.area, in.mouseX)));
					c.r.cursorChanged = true;
					return;
				}
				if (c.Over(pl.area)) {
					if (in.alt) {
						m.gesture = (uint8)NkTimelineGesture::Pan;
						m.gestureViewStart = m.viewStart;
						m.gestureViewEnd = m.viewEnd;
						m.gestureValueMin = m.valueMin;
						m.gestureValueMax = m.valueMax;
						return;
					}
					nk_uint64 piste = 0;
					int32 cle = -1, canal = -1;
					if (m.curveMode) {
						PointSous(c, piste, cle, canal);
					} else if (const NkTimelineRow *l = LigneSous(c, in.mouseY)) {
						cle = CleSous(c, *l, piste);
						// Un losange de RESUME (en-tete d'objet) prend toutes les cles
						// de l'objet a ce temps.
						if (cle >= 0 && l->track == 0) {
							const float32 t = m.Track(piste)->keys[(uint32)cle].time;
							if (!ajout) {
								m.ClearSelection();
							}
							for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
								NkTimelineTrack &tr = m.tracks[i];
								if (tr.object == l->object) {
									const int32 k = m.KeyAt(tr, t);
									if (k >= 0) {
										tr.keys[(uint32)k].selected = true;
									}
								}
							}
							m.gesture = (uint8)NkTimelineGesture::MoveKeys;
							m.gestureT0 = NkTimelineXToTime(m, pl.area, in.mouseX);
							RetenirChoisies(c);
							return;
						}
					}
					if (cle >= 0) {
						NkTimelineTrack *tr = m.Track(piste);
						const bool deja = tr->keys[(uint32)cle].selected;
						if (!deja || in.ctrl) {
							m.SelectKey(piste, cle, ajout);
						}
						m.activeTrack = m.curveMode ? m.activeTrack : piste;
						m.gesture = (uint8)NkTimelineGesture::MoveKeys;
						m.gestureTrack = piste;
						m.gestureKey = cle;
						m.gestureChannel = canal;
						m.gestureT0 = NkTimelineXToTime(m, pl.area, in.mouseX);
						m.gestureV0 = NkTimelineYToValue(m, pl.area, in.mouseY);
						RetenirChoisies(c);
						return;
					}
					m.gesture = (uint8)NkTimelineGesture::Box;
					return;
				}
				if (c.Over(pl.list)) {
					const NkTimelineRow *l = LigneSous(c, in.mouseY);
					if (l == nullptr) {
						return;
					}
					if (l->track == 0) {
						m.ToggleCollapsed(l->object);
						return;
					}
					NkTimelineTrack *tr = m.Track(l->track);
					if (tr == nullptr) {
						return;
					}
					const ZonesLigne z = Zones(c, *l, *tr);
					if (c.Over(z.cle)) {
						float32 v[4];
						c.ValueToKey(*tr, m.cursor, v);
						m.SetKey(tr->id, m.cursor, v);
						c.r.changed = true;
						return;
					}
					if (c.Over(z.retirer)) {
						m.RemoveTrack(tr->id);
						c.r.trackRemoved = true;
						c.r.changed = true;
						return;
					}
					for (uint32 ch = 0; ch < tr->channels; ++ch) {
						if (m.curveMode && c.Over(z.bascule[ch])) {
							tr->showChannel[ch] = !tr->showChannel[ch];
							return;
						}
						if (c.Over(z.canal[ch])) {
							float32 v[4];
							c.ValueAtCursor(*tr, v);
							m.gesture = (uint8)NkTimelineGesture::ScrubValue;
							m.gestureTrack = tr->id;
							m.gestureChannel = (int32)ch;
							m.gestureV0 = v[ch];
							m.activeTrack = tr->id;
							return;
						}
					}
					// Un clic sur la piste ACTIVE la rend a toutes (courbes : on revoit tout).
					m.activeTrack = m.activeTrack == tr->id ? 0 : tr->id;
				}
			}

			void Tenu(Ctx &c) {
				NkTimelineModel &m = c.m;
				const NkComponentInput &in = c.in;
				const Plan &pl = c.plan;
				const float32 dx = in.mouseX - m.gestureX0, dy = in.mouseY - m.gestureY0;
				const float32 seuil = c.M("drag_px");
				if (!m.gestureMoved && dx * dx + dy * dy > seuil * seuil) {
					m.gestureMoved = true;
				}
				switch ((NkTimelineGesture)m.gesture) {
					case NkTimelineGesture::Scrub: {
						const float32 avant = m.cursor;
						m.SetCursor(m.Snap(NkTimelineXToTime(m, pl.area, in.mouseX)));
						c.r.cursorChanged = c.r.cursorChanged || m.cursor != avant;
						break;
					}
					case NkTimelineGesture::MoveKeys:
						if (m.gestureMoved) {
							if (!m.gestureSnapshot) {
								m.PushUndo();
								m.gestureSnapshot = true;
							}
							Glisser(c);
						}
						break;
					case NkTimelineGesture::Pan: {
						const float32 span = m.gestureViewEnd - m.gestureViewStart;
						const float32 dtv = -dx / (pl.area.w > 1.f ? pl.area.w : 1.f) * span;
						m.viewStart = m.gestureViewStart + dtv;
						m.viewEnd = m.gestureViewEnd + dtv;
						if (m.curveMode) {
							const float32 vspan = m.gestureValueMax - m.gestureValueMin;
							const float32 dvv = dy / (pl.area.h > 1.f ? pl.area.h : 1.f) * vspan;
							m.valueMin = m.gestureValueMin + dvv;
							m.valueMax = m.gestureValueMax + dvv;
							m.curveAutoFit = false;
						}
						break;
					}
					case NkTimelineGesture::ScrubValue:
						if (m.gestureMoved) {
							NkTimelineTrack *tr = m.Track(m.gestureTrack);
							if (tr == nullptr || m.gestureChannel < 0) {
								break;
							}
							if (!m.gestureSnapshot) {
								m.PushUndo();
								m.gestureSnapshot = true;
							}
							float32 v[4];
							c.ValueAtCursor(*tr, v);
							const float32 pas = tr->kind == NkTimelineValueKind::Couleur ? c.Raw("scrub_color_px")
																						 : c.Raw("scrub_px");
							float32 nv = m.gestureV0 + dx * pas;
							if (tr->kind == NkTimelineValueKind::Couleur) {
								nv = nv < 0.f ? 0.f : (nv > 1.f ? 1.f : nv);
							} else if (tr->kind == NkTimelineValueKind::Palier) {
								nv = std::floor(m.gestureV0 + dx * c.Raw("scrub_px") * 10.f + 0.5f);
							}
							v[m.gestureChannel] = nv;
							m.SetKey(tr->id, m.cursor, v, 255, false);
							c.r.changed = true;
						}
						break;
					default:
						break;
				}
			}

			void Relache(Ctx &c) {
				NkTimelineModel &m = c.m;
				switch ((NkTimelineGesture)m.gesture) {
					case NkTimelineGesture::MoveKeys:
						if (m.gestureMoved) {
							m.Normalize();
							c.r.changed = true;
						} else if (!c.in.ctrl && !c.in.shift && m.gestureKey >= 0) {
							// Un CLIC sur une cle deja choisie (sans glisser) la choisit seule.
							m.SelectKey(m.gestureTrack, m.gestureKey, false);
						}
						break;
					case NkTimelineGesture::Box:
						if (m.gestureMoved) {
							BoiteChoisir(c, c.in.ctrl || c.in.shift);
						} else if (!c.in.ctrl && !c.in.shift) {
							m.ClearSelection();
						}
						break;
					default:
						break;
				}
				m.gesture = (uint8)NkTimelineGesture::None;
				m.gestureKey = -1;
			}

			void DoubleClic(Ctx &c) {
				NkTimelineModel &m = c.m;
				if (!c.Over(c.plan.area)) {
					return;
				}
				const float32 t = m.Snap(NkTimelineXToTime(m, c.plan.area, c.in.mouseX));
				nk_uint64 piste = 0;
				if (m.curveMode) {
					piste = m.activeTrack;
				} else if (const NkTimelineRow *l = LigneSous(c, c.in.mouseY)) {
					piste = l->track;
				}
				NkTimelineTrack *tr = m.Track(piste);
				if (tr == nullptr) {
					return;
				}
				float32 v[4];
				if (!c.Eval(*tr, t, v)) {
					c.ValueToKey(*tr, t, v);
				}
				const int32 k = m.SetKey(tr->id, t, v);
				m.SelectKey(tr->id, k, false);
				// Le second appui du double-clic a ouvert un cadre : il ne doit pas,
				// au relache, defaire la selection de la cle qu'on vient de poser.
				m.gesture = (uint8)NkTimelineGesture::None;
				c.r.changed = true;
			}

			void Molette(Ctx &c) {
				NkTimelineModel &m = c.m;
				const NkComponentInput &in = c.in;
				if (in.wheel == 0.f) {
					return;
				}
				const Plan &pl = c.plan;
				const bool zone = c.Over(pl.area) || c.Over(pl.ruler);
				if (c.Over(pl.list) || (zone && in.shift)) {
					m.rowScroll += in.wheel > 0.f ? -1 : 1;
					return;
				}
				if (!zone) {
					return;
				}
				const float32 f = in.wheel > 0.f ? c.Raw("wheel_zoom") : 1.f / c.Raw("wheel_zoom");
				if (m.curveMode && in.ctrl) {
					const float32 pivot = NkTimelineYToValue(m, pl.area, in.mouseY);
					m.valueMin = pivot - (pivot - m.valueMin) * f;
					m.valueMax = pivot + (m.valueMax - pivot) * f;
					m.curveAutoFit = false;
					return;
				}
				m.ZoomTime(NkTimelineXToTime(m, pl.area, in.mouseX), f);
			}

			// ── 3. LE DESSIN ────────────────────────────────────────────────────
			void Bouton(Ctx &c, NkTimelineButton b, const char *texte, bool enfonce, bool actif) {
				const NkPaintRect &rc = c.r.buttons[(uint8)b];
				if (rc.w <= 0.f) {
					return;
				}
				const bool survol = c.Over(rc);
				if (survol) {
					c.r.hoveredButton = (int32)b;
				}
				c.p.Fill(rc, enfonce ? c.s.accent : (survol && actif ? c.s.inputBg : c.s.buttonBg), c.M("button_gap"));
				if (texte != nullptr) {
					c.p.Text(rc, texte, enfonce ? c.s.textOnAccent : (actif ? c.s.text : c.s.textMuted), NkTextAlign::Center);
				}
			}

			void IconeTransport(Ctx &c, NkTimelineButton b) {
				const NkPaintRect &rc = c.r.buttons[(uint8)b];
				if (rc.w <= 0.f) {
					return;
				}
				const float32 cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f;
				const float32 d = rc.h * 0.25f;
				const float32 ep = c.M("stroke_w") * 2.f;
				const uint16 role = c.s.text;
				switch (b) {
					case NkTimelineButton::First:
						c.p.Fill(NkPaintRect{cx - d, cy - d, ep, d * 2.f}, role);
						Triangle(c, cx + d, cy - d, cx + d, cy + d, cx - d + ep, cy, role);
						break;
					case NkTimelineButton::Last:
						c.p.Fill(NkPaintRect{cx + d - ep, cy - d, ep, d * 2.f}, role);
						Triangle(c, cx - d, cy - d, cx - d, cy + d, cx + d - ep, cy, role);
						break;
					case NkTimelineButton::PrevKey:
						Triangle(c, cx + d * 0.2f, cy - d, cx + d * 0.2f, cy + d, cx - d, cy, role);
						Losange(c, cx + d, cy, d * 0.6f, c.s.keySelected);
						break;
					case NkTimelineButton::NextKey:
						Losange(c, cx - d, cy, d * 0.6f, c.s.keySelected);
						Triangle(c, cx - d * 0.2f, cy - d, cx - d * 0.2f, cy + d, cx + d, cy, role);
						break;
					case NkTimelineButton::Play:
						if (c.m.playing) {
							c.p.Fill(NkPaintRect{cx - d, cy - d, d * 0.7f, d * 2.f}, c.s.textOnAccent);
							c.p.Fill(NkPaintRect{cx + d * 0.3f, cy - d, d * 0.7f, d * 2.f}, c.s.textOnAccent);
						} else {
							Triangle(c, cx - d * 0.8f, cy - d, cx - d * 0.8f, cy + d, cx + d, cy, role);
						}
						break;
					case NkTimelineButton::Loop: {
						// Deux fleches qui se suivent : un cadre ouvert et sa pointe.
						const uint16 rl = c.m.loop ? c.s.textOnAccent : c.s.textMuted;
						c.p.Line(cx - d, cy - d * 0.5f, cx + d, cy - d * 0.5f, rl, ep * 0.6f);
						c.p.Line(cx + d, cy + d * 0.5f, cx - d, cy + d * 0.5f, rl, ep * 0.6f);
						c.p.Line(cx - d, cy - d * 0.5f, cx - d, cy + d * 0.5f, rl, ep * 0.6f);
						Triangle(c, cx + d * 0.3f, cy - d, cx + d * 0.3f, cy, cx + d, cy - d * 0.5f, rl);
						break;
					}
					default:
						break;
				}
			}

			void DessinerBarre(Ctx &c) {
				const NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				c.p.Fill(pl.toolbar, c.s.headerBg);
				c.p.HLine(pl.toolbar.x, pl.toolbar.y + pl.toolbar.h - c.M("stroke_w"), pl.toolbar.w, c.s.border);
				const NkTimelineButton transport[] = {NkTimelineButton::First, NkTimelineButton::PrevKey,
													  NkTimelineButton::Play,  NkTimelineButton::NextKey,
													  NkTimelineButton::Last,  NkTimelineButton::Loop};
				for (NkTimelineButton b : transport) {
					const bool enfonce = (b == NkTimelineButton::Play && m.playing) || (b == NkTimelineButton::Loop && m.loop);
					Bouton(c, b, nullptr, enfonce, true);
					IconeTransport(c, b);
				}
				// Le compteur « image / dernière ».
				const NkPaintRect &boucle = c.r.buttons[(uint8)NkTimelineButton::Loop];
				char cpt[48];
				std::snprintf(cpt, sizeof(cpt), "%d / %d", m.FrameOf(m.cursor), m.FrameOf(m.duration));
				const float32 pad = c.M("pad");
				c.p.Text(NkPaintRect{boucle.x + boucle.w + pad, pl.toolbar.y, c.p.TextWidth("0000 / 0000") + pad, pl.toolbar.h},
						 cpt, c.s.text, NkTextAlign::Center);
				Bouton(c, NkTimelineButton::Mode, m.curveMode ? "Courbes" : "Feuille", m.curveMode, true);
				Bouton(c, NkTimelineButton::Snap, "Accrocher", m.snap, true);
				Bouton(c, NkTimelineButton::FrameAll, "Cadrer", false, true);
				const uint32 nb = m.SelectionCount();
				Bouton(c, NkTimelineButton::AddKey, "+ Cle", false, !m.tracks.Empty());
				Bouton(c, NkTimelineButton::Delete, "Suppr.", false, nb > 0);
				Bouton(c, NkTimelineButton::Undo, "Annuler", false, m.CanUndo());
				Bouton(c, NkTimelineButton::Redo, "Refaire", false, m.CanRedo());
				const uint8 commune = m.SelectionInterp();
				for (uint8 i = 0; i < (uint8)NkTimelineInterp::Count; ++i) {
					Bouton(c, (NkTimelineButton)((uint8)NkTimelineButton::InterpFirst + i), Court(i), commune == i, nb > 0);
				}
			}

			/// Le pas des graduations nommees, en images : le premier de la suite
			/// qui laisse `label_min_px` entre deux nombres.
			int32 PasGraduation(const Ctx &c) {
				const NkTimelineModel &m = c.m;
				const float32 span = m.viewEnd - m.viewStart;
				const float32 pxImage = c.plan.area.w / (span * m.fps > 1e-3f ? span * m.fps : 1e-3f);
				static const int32 kPas[] = {1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1200, 3000, 6000};
				for (int32 p : kPas) {
					if ((float32)p * pxImage >= c.M("label_min_px")) {
						return p;
					}
				}
				return 6000;
			}

			void DessinerRegle(Ctx &c) {
				const NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				c.p.Fill(pl.ruler, c.s.headerBg);
				c.p.Fill(pl.listHead, c.s.headerBg);
				c.p.HLine(pl.ruler.x, pl.ruler.y + pl.ruler.h - c.M("stroke_w"), pl.ruler.w, c.s.border);
				c.p.HLine(pl.listHead.x, pl.listHead.y + pl.listHead.h - c.M("stroke_w"), pl.listHead.w, c.s.border);
				Bouton(c, NkTimelineButton::AddTrack, "+ Propriete", false, true);
				const int32 pas = PasGraduation(c);
				const float32 f = m.FrameDuration();
				const int32 i0 = (int32)std::floor(m.viewStart / f);
				const int32 i1 = (int32)std::ceil(m.viewEnd / f);
				const int32 mineur = pas >= 5 ? pas / 5 : 1;
				c.p.PushClip(pl.ruler);
				char txt[24];
				for (int32 i = i0 - (i0 % mineur) - mineur; i <= i1; i += mineur) {
					if (i < 0) {
						continue;
					}
					const float32 x = NkTimelineTimeToX(m, pl.area, (float32)i * f);
					if (i % pas == 0) {
						c.p.VLine(x, pl.ruler.y + pl.ruler.h * 0.5f, pl.ruler.h * 0.5f, c.s.textMuted);
						std::snprintf(txt, sizeof(txt), "%d", i);
						c.p.Text(NkPaintRect{x + c.M("button_gap") * 2.f, pl.ruler.y, c.M("label_min_px"), pl.ruler.h * 0.6f}, txt,
								 c.s.textMuted, NkTextAlign::Left);
					} else {
						c.p.VLine(x, pl.ruler.y + pl.ruler.h * 0.75f, pl.ruler.h * 0.25f, c.s.border);
					}
				}
				c.p.PopClip();
			}

			void DessinerFond(Ctx &c) {
				const NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				c.p.Fill(pl.list, c.s.panelBg);
				c.p.Fill(pl.area, c.s.panelBg);
				c.p.VLine(pl.area.x - c.M("stroke_w"), pl.ruler.y, pl.ruler.h + pl.area.h, c.s.border);
				// Les lignes : en-tetes d'objet sur toute la largeur, une piste sur deux.
				for (uint32 k = 0; k < (uint32)c.r.rows.Size() && !m.curveMode; ++k) {
					const NkTimelineRow &l = c.r.rows[k];
					if (l.track == 0) {
						c.p.Fill(NkPaintRect{pl.area.x, l.y, pl.area.w, l.h}, c.s.headerBg);
					} else if (k % 2 == 1) {
						c.p.Fill(NkPaintRect{pl.area.x, l.y, pl.area.w, l.h}, c.s.rowAltBg);
					}
				}
				// Les lignes d'images, aux graduations nommees.
				const int32 pas = PasGraduation(c);
				const float32 f = m.FrameDuration();
				c.p.PushClip(pl.area);
				for (int32 i = (int32)std::floor(m.viewStart / f / (float32)pas) * pas; (float32)i * f <= m.viewEnd; i += pas) {
					if (i >= 0) {
						c.p.VLine(NkTimelineTimeToX(m, pl.area, (float32)i * f), pl.area.y, pl.area.h, c.s.grid);
					}
				}
				// Au-dela de la duree (et avant zero) : un voile.
				const float32 x0 = NkTimelineTimeToX(m, pl.area, 0.f);
				const float32 xd = NkTimelineTimeToX(m, pl.area, m.duration);
				if (x0 > pl.area.x) {
					c.p.FillColor(NkPaintRect{pl.area.x, pl.area.y, x0 - pl.area.x, pl.area.h}, Voile(c.p, c.s.border, 0x70u));
				}
				if (xd < pl.area.x + pl.area.w) {
					c.p.FillColor(NkPaintRect{xd, pl.area.y, pl.area.x + pl.area.w - xd, pl.area.h},
								  Voile(c.p, c.s.border, 0x70u));
					c.p.VLine(xd, pl.area.y, pl.area.h, c.s.textMuted);
				}
				c.p.PopClip();
			}

			void DessinerColonne(Ctx &c) {
				NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				const float32 pad = c.M("pad");
				c.p.PushClip(pl.list);
				for (uint32 k = 0; k < (uint32)c.r.rows.Size(); ++k) {
					const NkTimelineRow &l = c.r.rows[k];
					const NkPaintRect ligne{pl.list.x, l.y, pl.list.w, l.h};
					if (l.track == 0) {
						c.p.Fill(ligne, c.s.headerBg);
						// Le chevron, TRACE : ▸ replie, ▾ ouvert.
						const float32 cx = ligne.x + pad + c.M("indent") * 0.4f, cy = l.y + l.h * 0.5f;
						const float32 d = l.h * 0.18f;
						if (m.IsCollapsed(l.object)) {
							Triangle(c, cx - d * 0.6f, cy - d, cx - d * 0.6f, cy + d, cx + d, cy, c.s.textMuted);
						} else {
							Triangle(c, cx - d, cy - d * 0.6f, cx + d, cy - d * 0.6f, cx, cy + d, c.s.textMuted);
						}
						c.p.Text(NkPaintRect{ligne.x + pad + c.M("indent"), l.y, ligne.w - pad * 2.f - c.M("indent"), l.h},
								 NomObjet(l.object), c.s.text, NkTextAlign::Left);
						continue;
					}
					const NkTimelineTrack *tr = m.Track(l.track);
					if (tr == nullptr) {
						continue;
					}
					const bool active = m.activeTrack == tr->id;
					if (active) {
						c.p.Fill(ligne, c.s.accent);
					} else if (k % 2 == 1) {
						c.p.Fill(ligne, c.s.rowAltBg);
					}
					if (c.Over(ligne)) {
						c.r.hoveredTrack = tr->id;
					}
					const ZonesLigne z = Zones(c, l, *tr);
					c.p.Text(z.libelle, tr->label.Empty() ? tr->property.CStr() : tr->label.CStr(),
							 active ? c.s.textOnAccent : c.s.text, NkTextAlign::Left);
					// Les valeurs au curseur, une case par canal, liseree de sa couleur.
					if (NkTimelineParam(c.s, "show_values") > 0.5f) {
						float32 v[4];
						c.ValueAtCursor(*tr, v);
						for (uint32 ch = 0; ch < tr->channels; ++ch) {
							const NkPaintRect &cz = z.canal[ch];
							c.p.Fill(cz, c.s.inputBg, c.M("button_gap"));
							const bool visible = !m.curveMode || tr->showChannel[ch];
							c.p.Fill(z.bascule[ch], visible ? c.s.channel[ch] : c.s.border);
							char txt[24];
							if (tr->kind == NkTimelineValueKind::Palier) {
								std::snprintf(txt, sizeof(txt), "%.0f", (double)v[ch]);
							} else {
								std::snprintf(txt, sizeof(txt), "%.2f", (double)v[ch]);
							}
							c.p.Text(NkPaintRect{cz.x + c.M("button_gap") * 3.f, cz.y, cz.w - c.M("button_gap") * 4.f, cz.h}, txt,
									 c.s.text, NkTextAlign::Right);
						}
						// Une couleur : sa pastille, avant les canaux.
						if (tr->kind == NkTimelineValueKind::Couleur && tr->channels >= 3) {
							const float32 sw = l.h - c.M("button_gap") * 4.f;
							const NkPaintRect pastille{z.canal[0].x - sw - c.M("button_gap") * 2.f, l.y + c.M("button_gap") * 2.f, sw, sw};
							auto octet = [](float32 x) {
								x = x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
								return (uint32)(x * 255.f + 0.5f);
							};
							const uint32 a = tr->channels >= 4 ? octet(v[3]) : 255u;
							c.p.FillColor(pastille, (octet(v[0]) << 24) | (octet(v[1]) << 16) | (octet(v[2]) << 8) | a,
										  c.M("button_gap"));
						}
					}
					// « ◆ » : plein s'il y a une cle au curseur.
					const bool cleIci = m.KeyAt(*tr, m.cursor) >= 0;
					const float32 kr = c.M("key_r");
					Losange(c, z.cle.x + z.cle.w * 0.5f, z.cle.y + z.cle.h * 0.5f, kr, cleIci ? c.s.keySelected : c.s.key);
					if (!cleIci) {
						Losange(c, z.cle.x + z.cle.w * 0.5f, z.cle.y + z.cle.h * 0.5f, kr * 0.5f, c.s.panelBg);
					}
					// « − » au survol de la ligne.
					if (c.Over(ligne)) {
						const float32 cx = z.retirer.x + z.retirer.w * 0.5f, cy = z.retirer.y + z.retirer.h * 0.5f;
						c.p.Line(cx - kr, cy, cx + kr, cy, c.Over(z.retirer) ? c.s.text : c.s.textMuted, c.M("stroke_w") * 2.f);
					}
				}
				c.p.PopClip();
			}

			void DessinerLosanges(Ctx &c) {
				const NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				const float32 kr = c.M("key_r");
				const bool resume = NkTimelineParam(c.s, "show_summary") > 0.5f;
				c.p.PushClip(pl.area);
				for (uint32 k = 0; k < (uint32)c.r.rows.Size(); ++k) {
					const NkTimelineRow &l = c.r.rows[k];
					const float32 cy = l.y + l.h * 0.5f;
					if (l.track == 0) {
						if (!resume) {
							continue;
						}
						// Le RESUME : un petit losange par image ou l'objet a une cle.
						for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
							const NkTimelineTrack &tr = m.tracks[i];
							if (!(tr.object == l.object)) {
								continue;
							}
							for (uint32 q = 0; q < (uint32)tr.keys.Size(); ++q) {
								const float32 x = NkTimelineTimeToX(m, pl.area, tr.keys[q].time);
								Losange(c, x, cy, kr * 0.75f, tr.keys[q].selected ? c.s.keySelected : c.s.textMuted);
							}
						}
						continue;
					}
					const NkTimelineTrack *tr = m.Track(l.track);
					if (tr == nullptr) {
						continue;
					}
					for (uint32 q = 0; q < (uint32)tr->keys.Size(); ++q) {
						const NkTimelineKey &key = tr->keys[q];
						const float32 x = NkTimelineTimeToX(m, pl.area, key.time);
						const bool survol = Abs(x - c.in.mouseX) <= c.M("hit_r") && Abs(cy - c.in.mouseY) <= c.M("hit_r");
						if (survol) {
							c.r.hoveredTrack = tr->id;
							c.r.hoveredKey = (int32)q;
						}
						Losange(c, x, cy, kr + (survol ? c.M("stroke_w") : 0.f), c.s.border);
						Losange(c, x, cy, kr - c.M("stroke_w") + (survol ? c.M("stroke_w") : 0.f),
								key.selected ? c.s.keySelected : c.s.key);
						// Palier : un trait plat jusqu'a la cle suivante dit « la valeur tient ».
						if (q + 1 < (uint32)tr->keys.Size() && key.interp == (uint8)NkTimelineInterp::Palier) {
							const float32 x2 = NkTimelineTimeToX(m, pl.area, tr->keys[q + 1].time);
							c.p.HLine(x + kr, cy, x2 - x - kr * 2.f, c.s.border);
						}
					}
				}
				c.p.PopClip();
			}

			void AjusterValeurs(Ctx &c) {
				NkTimelineModel &m = c.m;
				if (!m.curveAutoFit || m.gesture == (uint8)NkTimelineGesture::MoveKeys) {
					return;
				}
				float32 lo = 1e30f, hi = -1e30f;
				for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
					const NkTimelineTrack &tr = m.tracks[i];
					if (!CourbeMontree(m, tr)) {
						continue;
					}
					for (uint32 k = 0; k < (uint32)tr.keys.Size(); ++k) {
						for (uint32 ch = 0; ch < tr.channels; ++ch) {
							if (tr.showChannel[ch]) {
								lo = tr.keys[k].v[ch] < lo ? tr.keys[k].v[ch] : lo;
								hi = tr.keys[k].v[ch] > hi ? tr.keys[k].v[ch] : hi;
							}
						}
					}
				}
				if (lo > hi) {
					lo = -1.f;
					hi = 1.f;
				}
				if (hi - lo < 1e-3f) {
					lo -= 1.f;
					hi += 1.f;
				}
				const float32 marge = (hi - lo) * 0.12f;
				m.valueMin = lo - marge;
				m.valueMax = hi + marge;
			}

			void DessinerCourbes(Ctx &c) {
				const NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				c.p.PushClip(pl.area);
				// Les lignes de VALEUR : un pas rond qui en donne cinq a dix.
				const float32 span = m.valueMax - m.valueMin;
				float32 pas = std::pow(10.f, std::floor(std::log10(span > 1e-6f ? span : 1e-6f)));
				if (span / pas < 4.f) {
					pas *= 0.5f;
				}
				char txt[24];
				for (float32 v = std::ceil(m.valueMin / pas) * pas; v <= m.valueMax; v += pas) {
					const float32 y = NkTimelineValueToY(m, pl.area, v);
					c.p.HLine(pl.area.x, y, pl.area.w, Abs(v) < pas * 0.01f ? c.s.textMuted : c.s.grid);
					std::snprintf(txt, sizeof(txt), "%.2f", (double)v);
					c.p.Text(NkPaintRect{pl.area.x + c.M("pad"), y - c.M("row_h"), c.M("value_w") * 2.f, c.M("row_h")}, txt,
							 c.s.textMuted, NkTextAlign::Left);
				}
				const float32 ech = c.M("curve_samples_px") > 1.f ? c.M("curve_samples_px") : 1.f;
				const float32 kr = c.M("key_r");
				for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
					const NkTimelineTrack &tr = m.tracks[i];
					if (!CourbeMontree(m, tr) || tr.keys.Empty()) {
						continue;
					}
					for (uint32 ch = 0; ch < tr.channels; ++ch) {
						if (!tr.showChannel[ch]) {
							continue;
						}
						// La courbe ECHANTILLONNEE par l'interpolation de l'hote : celle
						// que le jeu jouera, pas une approximation du kit.
						float32 px = pl.area.x, py = 0.f;
						for (float32 x = pl.area.x; x <= pl.area.x + pl.area.w + ech; x += ech) {
							float32 v[4];
							c.Eval(tr, NkTimelineXToTime(m, pl.area, x), v);
							const float32 y = NkTimelineValueToY(m, pl.area, v[ch]);
							if (x > pl.area.x) {
								c.p.Line(px, py, x, y, c.s.channel[ch], c.M("curve_w"));
							}
							px = x;
							py = y;
						}
						for (uint32 k = 0; k < (uint32)tr.keys.Size(); ++k) {
							const NkTimelineKey &key = tr.keys[k];
							const float32 x = NkTimelineTimeToX(m, pl.area, key.time);
							const float32 y = NkTimelineValueToY(m, pl.area, key.v[ch]);
							c.p.Ellipse(NkPaintRect{x - kr, y - kr, kr * 2.f, kr * 2.f}, c.s.border);
							const float32 r2 = kr - c.M("stroke_w");
							c.p.Ellipse(NkPaintRect{x - r2, y - r2, r2 * 2.f, r2 * 2.f}, key.selected ? c.s.keySelected : c.s.text);
						}
					}
				}
				c.p.PopClip();
			}

			void DessinerCurseur(Ctx &c) {
				const NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				const float32 x = NkTimelineTimeToX(m, pl.area, m.cursor);
				if (x < pl.area.x - 1.f || x > pl.area.x + pl.area.w + 1.f) {
					return;
				}
				const float32 ep = c.M("playhead_w");
				c.p.Fill(NkPaintRect{x - ep * 0.5f, pl.ruler.y, ep, pl.ruler.h + pl.area.h}, c.s.playhead);
				char txt[16];
				std::snprintf(txt, sizeof(txt), "%d", m.FrameOf(m.cursor));
				const float32 w = c.p.TextWidth(txt) + c.M("pad") * 2.f;
				const NkPaintRect tete{x - w * 0.5f, pl.ruler.y + c.M("button_gap"), w, pl.ruler.h - c.M("button_gap") * 2.f};
				c.p.Fill(tete, c.s.playhead, c.M("button_gap"));
				c.p.Text(tete, txt, c.s.textOnAccent, NkTextAlign::Center);
			}

			void DessinerBoite(Ctx &c) {
				const NkTimelineModel &m = c.m;
				if (m.gesture != (uint8)NkTimelineGesture::Box || !m.gestureMoved) {
					return;
				}
				const float32 x0 = m.gestureX0 < c.in.mouseX ? m.gestureX0 : c.in.mouseX;
				const float32 y0 = m.gestureY0 < c.in.mouseY ? m.gestureY0 : c.in.mouseY;
				const NkPaintRect b{x0, y0, Abs(c.in.mouseX - m.gestureX0), Abs(c.in.mouseY - m.gestureY0)};
				c.p.PushClip(c.plan.area);
				c.p.FillColor(b, Voile(c.p, c.s.accent, 0x30u));
				c.p.HLine(b.x, b.y, b.w, c.s.accent);
				c.p.HLine(b.x, b.y + b.h, b.w, c.s.accent);
				c.p.VLine(b.x, b.y, b.h, c.s.accent);
				c.p.VLine(b.x + b.w, b.y, b.h, c.s.accent);
				c.p.PopClip();
			}

		} // namespace

		NkTimelineResult NkDrawTimeline(NkComponentPaint &p, const NkComponentInput &in, const NkPaintRect &rect,
										NkTimelineModel &m, const NkTimelineStyle &s, const NkTimelineHooks &hooks) {
			NkTimelineResult r;
			Ctx c{p, in, m, s, hooks, r};
			c.S = in.surfaceScale > 0.f ? in.surfaceScale : 1.f;
			if (rect.w <= 0.f || rect.h <= 0.f) {
				return r;
			}
			// 1. La geometrie.
			Planifier(c, rect);
			Lignes(c);
			PlacerBoutons(c);
			// 2. Les gestes, AVANT le dessin.
			if (in.mousePressed && m.gesture == (uint8)NkTimelineGesture::None) {
				Appui(c);
			} else if (in.mouseDown && m.gesture != (uint8)NkTimelineGesture::None) {
				Tenu(c);
			}
			if (in.mouseReleased && m.gesture != (uint8)NkTimelineGesture::None) {
				Tenu(c);
				Relache(c);
			}
			if (in.doubleClick) {
				DoubleClic(c);
			}
			Molette(c);
			// Les lignes ont pu changer (un objet replie, une piste retiree).
			r.rows.Clear();
			Lignes(c);
			for (uint32 k = 0; k < (uint32)r.rows.Size(); ++k) {
				NkTimelineRow &l = r.rows[k];
				if (const NkTimelineTrack *tr = m.Track(l.track)) {
					const ZonesLigne z = Zones(c, l, *tr);
					l.keyButton = z.cle;
					for (uint32 ch = 0; ch < 4; ++ch) {
						l.channel[ch] = z.canal[ch];
					}
				}
			}
			if (m.curveMode) {
				AjusterValeurs(c);
			}
			// 3. Le dessin.
			DessinerFond(c);
			DessinerRegle(c);
			DessinerColonne(c);
			if (m.curveMode) {
				DessinerCourbes(c);
			} else {
				DessinerLosanges(c);
			}
			DessinerBoite(c);
			DessinerCurseur(c);
			DessinerBarre(c);
			return r;
		}

	} // namespace editorkit
} // namespace nkentseu
