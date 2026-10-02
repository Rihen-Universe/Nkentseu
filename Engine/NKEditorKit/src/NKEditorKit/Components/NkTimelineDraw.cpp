// -----------------------------------------------------------------------------
// @File    NkTimelineDraw.cpp
// @Brief   Le dessin et les gestes de la frise (feuille d'images, courbes,
//          pistes de clips), a la maniere du Sequencer d'Unreal 5 et de la
//          Dope Sheet / du NLA de Blender.
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
//   1. `Plan`  — la geometrie (barre, colonne, regle, zone, lignes, barre de
//      lecture du bas), UNE fois : le dessin et les gestes la lisent tous deux.
//   2. les gestes qui COMMENCENT (appui), CONTINUENT (bouton tenu), FINISSENT
//      (relache) -- ils modifient le modele AVANT le dessin, qui montre donc
//      deja l'etat de cette trame ;
//   3. le dessin.
//
// =============================================================================
//  LA DISPOSITION (2026-10-01 au soir, « comme Unreal 5 ou Blender »)
// =============================================================================
//   ┌ barre d'outils : Feuille|Courbes, Accrocher, Cadrer · ◆+ ✕ ⧉ 📋 ⚑ · ↶ ↷ ·
//   │                  tangentes · interpolations (icones)      30 im/s
//   ├ « + Piste »  [compteur]  │ regle : secondes / images, plage, marqueurs
//   ├ ARBRE des pistes          │ cles (formes), sections, clips, courbes
//   │  ▾ objet      M S 🔒      │
//   │     piste  1.0 ◁◆▷ ✕ M S 🔒│
//   └ |◀ ◀◆ ◀| ▶ |▶ ◆▶ ▶| ⟳ 1x 12/30  │ barre de defilement (bords = zoom)
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
			float32 Min(float32 a, float32 b) {
				return a < b ? a : b;
			}
			float32 Max(float32 a, float32 b) {
				return a > b ? a : b;
			}

			/// Le texte des infobulles (le kit les peint lui-meme : la barre est
			/// faite d'icones, comme celle d'UE5).
			const char *Bulle(uint8 b) {
				switch ((NkTimelineButton)b) {
					case NkTimelineButton::First:
						return "Debut";
					case NkTimelineButton::PrevKey:
						return "Cle precedente";
					case NkTimelineButton::Play:
						return "Lecture / pause";
					case NkTimelineButton::NextKey:
						return "Cle suivante";
					case NkTimelineButton::Last:
						return "Fin";
					case NkTimelineButton::Loop:
						return "Lecture en boucle";
					case NkTimelineButton::Mode:
						return "Feuille d'images / editeur de courbes";
					case NkTimelineButton::Snap:
						return "Accrocher aux images";
					case NkTimelineButton::FrameAll:
						return "Cadrer toute l'animation";
					case NkTimelineButton::AddKey:
						return "Cle au curseur (piste active)";
					case NkTimelineButton::Delete:
						return "Supprimer la selection";
					case NkTimelineButton::Undo:
						return "Annuler";
					case NkTimelineButton::Redo:
						return "Refaire";
					case NkTimelineButton::AddTrack:
						return "Ajouter une piste";
					case NkTimelineButton::PrevFrame:
						return "Image precedente";
					case NkTimelineButton::NextFrame:
						return "Image suivante";
					case NkTimelineButton::Copy:
						return "Copier les cles choisies";
					case NkTimelineButton::Paste:
						return "Coller au curseur";
					case NkTimelineButton::AddMarker:
						return "Marqueur au curseur";
					case NkTimelineButton::Speed:
						return "Vitesse de lecture";
					default:
						break;
				}
				if (b >= (uint8)NkTimelineButton::TangentFirst && b < (uint8)NkTimelineButton::InterpFirst) {
					static const char *const k[(uint8)NkTimelineTangent::Count] = {
						"Tangente automatique", "Tangente unifiee (a la main)", "Tangente cassee", "Tangente plate"};
					return k[b - (uint8)NkTimelineButton::TangentFirst];
				}
				if (b >= (uint8)NkTimelineButton::InterpFirst && b < (uint8)NkTimelineButton::Count) {
					return NkTimelineInterpName((uint8)(b - (uint8)NkTimelineButton::InterpFirst));
				}
				return nullptr;
			}

			struct Plan {
					NkPaintRect toolbar, list, listHead, ruler, area, transport, transportLeft, scroll;
					float32 rowH = 0.f, groupH = 0.f, clipH = 0.f;
			};

			/// Les sous-zones d'une ligne de la colonne.
			struct ZonesLigne {
					NkPaintRect chevron, icone, libelle;
					NkPaintRect canal[4], bascule[4];
					NkPaintRect prec, cle, suiv, retirer;
					NkPaintRect muet, solo, verrou;
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
					/// Un role facultatif du style, ou son repli.
					uint16 Role(uint16 role, uint16 repli) const {
						return role != 0 ? role : repli;
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

			/// Un polygone plein ; repli VISIBLE (un carre) si le peintre n'en sait rien.
			void Poly(Ctx &c, const float32 *xy, int32 n, uint32 rgba, float32 cx, float32 cy, float32 r) {
				if (!c.p.PolygonHex(xy, n, rgba)) {
					c.p.FillColor(NkPaintRect{cx - r * 0.7f, cy - r * 0.7f, r * 1.4f, r * 1.4f}, rgba);
				}
			}

			void Losange(Ctx &c, float32 cx, float32 cy, float32 r, uint16 role) {
				const float32 xy[8] = {cx, cy - r, cx + r, cy, cx, cy + r, cx - r, cy};
				Poly(c, xy, 4, c.p.ColorOf(role), cx, cy, r);
			}

			/// Un triangle plein (icones), sommets en paires.
			void Triangle(Ctx &c, float32 x0, float32 y0, float32 x1, float32 y1, float32 x2, float32 y2, uint16 role) {
				const float32 xy[6] = {x0, y0, x1, y1, x2, y2};
				if (!c.p.PolygonHex(xy, 3, c.p.ColorOf(role))) {
					c.p.Line(x0, y0, x1, y1, role, c.M("stroke_w"));
					c.p.Line(x1, y1, x2, y2, role, c.M("stroke_w"));
					c.p.Line(x2, y2, x0, y0, role, c.M("stroke_w"));
				}
			}

			/// LA FORME D'UNE CLE DIT SON INTERPOLATION (comme Blender) :
			///   palier = carre · lineaire = losange · courbe = rond ·
			///   douceurs (entree, sortie, entree-sortie) = octogone ·
			///   dynamiques (rebond, elastique, recul) = hexagone allonge.
			void FormeCle(Ctx &c, float32 cx, float32 cy, float32 r, uint8 interp, uint32 rgba) {
				float32 xy[24];
				int32 n = 0;
				switch ((NkTimelineInterp)interp) {
					case NkTimelineInterp::Palier: {
						const float32 a = r * 0.78f;
						const float32 q[8] = {cx - a, cy - a, cx + a, cy - a, cx + a, cy + a, cx - a, cy + a};
						for (int32 i = 0; i < 8; ++i) {
							xy[i] = q[i];
						}
						n = 4;
						break;
					}
					case NkTimelineInterp::Courbe: {
						for (int32 i = 0; i < 12; ++i) {
							const float32 a = (float32)i * (6.2831853f / 12.f);
							xy[i * 2] = cx + std::cos(a) * r * 0.86f;
							xy[i * 2 + 1] = cy + std::sin(a) * r * 0.86f;
						}
						n = 12;
						break;
					}
					case NkTimelineInterp::Entree:
					case NkTimelineInterp::Sortie:
					case NkTimelineInterp::EntreeSortie: {
						for (int32 i = 0; i < 8; ++i) {
							const float32 a = (float32)i * (6.2831853f / 8.f) + 0.3926991f;
							xy[i * 2] = cx + std::cos(a) * r * 0.95f;
							xy[i * 2 + 1] = cy + std::sin(a) * r * 0.95f;
						}
						n = 8;
						break;
					}
					case NkTimelineInterp::Rebond:
					case NkTimelineInterp::Elastique:
					case NkTimelineInterp::Recul: {
						const float32 q[12] = {cx - r * 1.15f, cy,			   cx - r * 0.5f, cy - r * 0.8f,
											   cx + r * 0.5f,  cy - r * 0.8f, cx + r * 1.15f, cy,
											   cx + r * 0.5f,  cy + r * 0.8f, cx - r * 0.5f, cy + r * 0.8f};
						for (int32 i = 0; i < 12; ++i) {
							xy[i] = q[i];
						}
						n = 6;
						break;
					}
					default: {
						const float32 q[8] = {cx, cy - r, cx + r, cy, cx, cy + r, cx - r, cy};
						for (int32 i = 0; i < 8; ++i) {
							xy[i] = q[i];
						}
						n = 4;
						break;
					}
				}
				Poly(c, xy, n, rgba, cx, cy, r);
			}

			/// Une cle : un liseré sombre, le corps (clair, ambre si choisie).
			void Cle(Ctx &c, float32 cx, float32 cy, float32 r, uint8 interp, bool choisie, bool survol, bool terne) {
				const float32 ep = c.M("stroke_w");
				const float32 rr = r + (survol ? ep : 0.f);
				FormeCle(c, cx, cy, rr + ep, interp, Voile(c.p, c.s.panelBg, terne ? 0x80u : 0xFFu));
				const uint16 corps = choisie ? c.s.keySelected : c.s.key;
				FormeCle(c, cx, cy, rr, interp, Voile(c.p, corps, terne ? 0x60u : 0xFFu));
			}

			void Tirets(Ctx &c, float32 x, float32 y0, float32 y1, uint16 role) {
				const float32 pas = c.M("marker_h") * 0.5f;
				for (float32 y = y0; y < y1; y += pas * 2.f) {
					c.p.VLine(x, y, Min(pas, y1 - y), role);
				}
			}

			const char *NomCanal(const NkTimelineTrack &tr, uint32 ch) {
				if (ch < 4 && !tr.channelNames[ch].Empty()) {
					return tr.channelNames[ch].CStr();
				}
				static const char *const kXyzw[4] = {"X", "Y", "Z", "W"};
				static const char *const kRgba[4] = {"R", "G", "B", "A"};
				return tr.kind == NkTimelineValueKind::Couleur ? kRgba[ch & 3u] : kXyzw[ch & 3u];
			}

			/// Le dernier segment d'un chemin d'objet (« Bras/Main » -> « Main »).
			const char *NomObjet(const NkTimelineModel &m, const NkString &o) {
				if (o.Empty()) {
					return m.rootLabel.Empty() ? "(objet anime)" : m.rootLabel.CStr();
				}
				const char *s = o.CStr();
				const char *d = s;
				for (const char *q = s; *q != '\0'; ++q) {
					if (*q == '/') {
						d = q + 1;
					}
				}
				return d;
			}

			bool EstClips(const NkTimelineTrack &tr) {
				return tr.kind == NkTimelineValueKind::Clips;
			}

			// ── 1. LE PLAN ──────────────────────────────────────────────────────
			void Planifier(Ctx &c, const NkPaintRect &rect) {
				Plan &pl = c.plan;
				const float32 tb = c.M("toolbar_h");
				const float32 rh = c.M("ruler_h");
				const float32 th = c.M("transport_h");
				float32 lw = c.M("list_w");
				if (lw > rect.w * 0.5f) {
					lw = rect.w * 0.5f;
				}
				pl.toolbar = NkPaintRect{rect.x, rect.y, rect.w, tb};
				const float32 corpsY = rect.y + tb;
				float32 corpsH = rect.h - tb - th;
				corpsH = corpsH > 0.f ? corpsH : 0.f;
				pl.listHead = NkPaintRect{rect.x, corpsY, lw, rh};
				pl.list = NkPaintRect{rect.x, corpsY + rh, lw, corpsH - rh > 0.f ? corpsH - rh : 0.f};
				const float32 ax = rect.x + lw + c.M("stroke_w");
				const float32 aw = rect.x + rect.w - ax > 0.f ? rect.x + rect.w - ax : 0.f;
				pl.ruler = NkPaintRect{ax, corpsY, aw, rh};
				pl.area = NkPaintRect{ax, corpsY + rh, aw, pl.list.h};
				pl.transport = NkPaintRect{rect.x, corpsY + corpsH, rect.w, th};
				pl.transportLeft = NkPaintRect{rect.x, corpsY + corpsH, lw, th};
				const float32 g = c.M("button_gap") * 2.f;
				pl.scroll = NkPaintRect{ax, pl.transport.y + g * 1.5f, aw, th - g * 3.f};
				pl.rowH = c.M("row_h");
				pl.groupH = c.M("group_h");
				pl.clipH = c.M("clip_row_h");
				c.r.list = pl.list;
				c.r.ruler = pl.ruler;
				c.r.area = pl.area;
				c.r.transport = pl.transport;
				c.r.scrollbar = pl.scroll;
			}

			/// L'ARBRE des objets : un noeud par chemin, ses pistes, ses enfants,
			/// dans l'ordre de PREMIERE APPARITION (l'ordre de l'hote est garde).
			struct Noeud {
					NkString chemin;
					NkVector<uint32> pistes;
					NkVector<uint32> enfants;
			};

			uint32 NoeudDe(NkVector<Noeud> &arbre, const NkString &chemin) {
				for (uint32 i = 0; i < (uint32)arbre.Size(); ++i) {
					if (arbre[i].chemin == chemin) {
						return i;
					}
				}
				// Cree le parent d'abord : il precede toujours son enfant.
				uint32 parent = 0;
				const bool racine = chemin.Empty();
				if (!racine) {
					parent = NoeudDe(arbre, NkTimelineModel::ParentObject(chemin));
				}
				Noeud n;
				n.chemin = chemin;
				arbre.PushBack(n);
				const uint32 id = (uint32)arbre.Size() - 1;
				if (!racine) {
					arbre[parent].enfants.PushBack(id);
				}
				return id;
			}

			void Parcourir(Ctx &c, const NkVector<Noeud> &arbre, uint32 i, bool visible, NkVector<NkTimelineRow> &out) {
				const Noeud &n = arbre[i];
				const uint32 prof = NkTimelineModel::ObjectDepth(n.chemin);
				if (visible) {
					NkTimelineRow g;
					g.track = 0;
					g.object = n.chemin;
					g.h = c.plan.groupH;
					g.depth = prof;
					out.PushBack(g);
				}
				const bool ouvert = visible && !c.m.IsCollapsed(n.chemin);
				for (uint32 k = 0; ouvert && k < (uint32)n.pistes.Size(); ++k) {
					const NkTimelineTrack &tr = c.m.tracks[n.pistes[k]];
					NkTimelineRow l;
					l.track = tr.id;
					l.object = tr.object;
					l.h = EstClips(tr) ? c.plan.clipH : c.plan.rowH;
					l.depth = prof + 1;
					out.PushBack(l);
				}
				for (uint32 k = 0; k < (uint32)n.enfants.Size(); ++k) {
					Parcourir(c, arbre, n.enfants[k], ouvert, out);
				}
			}

			/// Les lignes visibles : l'arbre des objets, puis `rowScroll` saute
			/// les premieres.
			void Lignes(Ctx &c) {
				NkTimelineModel &m = c.m;
				NkVector<NkTimelineRow> toutes;
				if (!m.tracks.Empty()) {
					NkVector<Noeud> arbre;
					NoeudDe(arbre, NkString());
					for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
						arbre[NoeudDe(arbre, m.tracks[i].object)].pistes.PushBack(i);
					}
					Parcourir(c, arbre, 0, true, toutes);
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

			ZonesLigne Zones(const Ctx &c, const NkTimelineRow &l, const NkTimelineTrack *tr) {
				ZonesLigne z;
				const NkPaintRect &li = c.plan.list;
				const float32 pad = c.M("pad");
				const float32 iw = c.M("icon_w");
				const float32 g = c.M("button_gap");
				float32 x = li.x + li.w - pad;
				auto poser = [&](NkPaintRect &rc) {
					x -= iw;
					rc = NkPaintRect{x, l.y + g, iw - g, l.h - g * 2.f};
				};
				poser(z.verrou);
				poser(z.solo);
				poser(z.muet);
				x -= g * 2.f;
				if (tr != nullptr) {
					poser(z.retirer);
					x -= g * 2.f;
					poser(z.suiv);
					poser(z.cle);
					poser(z.prec);
				}
				const uint32 prof = l.depth < 10u ? l.depth : 10u;
				const float32 retrait = li.x + pad + c.M("indent") * (float32)prof;
				z.chevron = NkPaintRect{retrait, l.y, c.M("indent"), l.h};
				z.icone = NkPaintRect{retrait + c.M("indent"), l.y, c.M("indent"), l.h};
				const float32 lx = z.icone.x + z.icone.w + g * 2.f;
				if (tr != nullptr && !EstClips(*tr) && NkTimelineParam(c.s, "show_values") > 0.5f) {
					// Les valeurs : ce qui reste apres un libelle lisible.
					const float32 libMin = c.M("value_w") * 1.5f;
					float32 vw = c.M("value_w");
					const float32 place = x - g * 2.f - (lx + libMin);
					if (vw * (float32)tr->channels > place) {
						vw = place > 0.f ? place / (float32)tr->channels : 0.f;
					}
					for (int32 ch = (int32)tr->channels - 1; ch >= 0 && vw > g * 6.f; --ch) {
						x -= vw;
						z.canal[ch] = NkPaintRect{x, l.y + g, vw - g, l.h - g * 2.f};
						z.bascule[ch] = NkPaintRect{x, l.y + g, g * 2.f, l.h - g * 2.f};
					}
				} else if (tr != nullptr && EstClips(*tr)) {
					// Une piste de clips : son influence, frottable.
					const float32 vw = c.M("value_w");
					x -= vw;
					z.canal[0] = NkPaintRect{x, l.y + g * 2.f, vw - g, l.h - g * 4.f};
				}
				z.libelle = NkPaintRect{lx, l.y, x - g * 2.f - lx > 0.f ? x - g * 2.f - lx : 0.f, l.h};
				return z;
			}

			// ── LA BARRE D'OUTILS ET LA BARRE DE LECTURE : la geometrie ─────────
			void PlacerBoutons(Ctx &c) {
				const Plan &pl = c.plan;
				const float32 bw = c.M("button_w");
				const float32 gap = c.M("button_gap");
				const float32 grp = c.M("group_gap");
				const float32 pad = c.M("pad");
				auto texte = [&](const char *t) { return c.p.TextWidth(t) + pad * 2.f; };
				// La barre du HAUT : l'edition.
				{
					const float32 by = pl.toolbar.y + gap * 2.f;
					const float32 bh = pl.toolbar.h - gap * 4.f;
					float32 x = pl.toolbar.x + pad;
					const float32 fin = pl.toolbar.x + pl.toolbar.w - pad - texte("000 im/s");
					auto poser = [&](NkTimelineButton b, float32 w) {
						NkPaintRect rc{x, by, w, bh};
						if (x + w > fin) {
							rc.w = 0.f; // hors de la barre : ni dessine, ni cliquable
						}
						c.r.buttons[(uint8)b] = rc;
						x += w + gap;
					};
					poser(NkTimelineButton::Mode, texte("Feuille") + texte("Courbes"));
					x += gap;
					poser(NkTimelineButton::Snap, bw + texte("Accrocher") - pad);
					poser(NkTimelineButton::FrameAll, texte("Cadrer"));
					x += grp;
					poser(NkTimelineButton::AddKey, bw);
					poser(NkTimelineButton::Delete, bw);
					poser(NkTimelineButton::Copy, bw);
					poser(NkTimelineButton::Paste, bw);
					poser(NkTimelineButton::AddMarker, bw);
					x += grp;
					poser(NkTimelineButton::Undo, bw);
					poser(NkTimelineButton::Redo, bw);
					x += grp;
					for (uint8 i = 0; i < (uint8)NkTimelineTangent::Count; ++i) {
						poser((NkTimelineButton)((uint8)NkTimelineButton::TangentFirst + i), bw);
					}
					x += grp;
					for (uint8 i = 0; i < (uint8)NkTimelineInterp::Count; ++i) {
						poser((NkTimelineButton)((uint8)NkTimelineButton::InterpFirst + i), bw);
					}
				}
				// La barre du BAS, sous la colonne : la lecture (Sequencer d'UE5).
				{
					const NkPaintRect &tl = pl.transportLeft;
					const float32 by = tl.y + gap * 2.f;
					const float32 bh = tl.h - gap * 4.f;
					float32 x = tl.x + pad;
					const float32 fin = tl.x + tl.w - pad;
					auto poser = [&](NkTimelineButton b, float32 w) {
						NkPaintRect rc{x, by, w, bh};
						if (x + w > fin) {
							rc.w = 0.f;
						}
						c.r.buttons[(uint8)b] = rc;
						x += w + gap;
					};
					poser(NkTimelineButton::First, bw);
					poser(NkTimelineButton::PrevKey, bw);
					poser(NkTimelineButton::PrevFrame, bw);
					poser(NkTimelineButton::Play, bw);
					poser(NkTimelineButton::NextFrame, bw);
					poser(NkTimelineButton::NextKey, bw);
					poser(NkTimelineButton::Last, bw);
					poser(NkTimelineButton::Loop, bw);
					poser(NkTimelineButton::Speed, texte("0.25x"));
				}
				// « + Piste » vit en tete de la colonne, au-dessus de l'arbre.
				const float32 aw = c.p.TextWidth("+ Piste") + pad * 2.f;
				c.r.buttons[(uint8)NkTimelineButton::AddTrack] =
					NkPaintRect{pl.listHead.x + pad, pl.listHead.y + gap * 3.f, aw, pl.listHead.h - gap * 6.f};
			}

			// ── 2. LES GESTES ───────────────────────────────────────────────────
			/// La cle sous le curseur dans une ligne (feuille d'images), -1 sinon.
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
					if (tr.locked || EstClips(tr)) {
						continue;
					}
					if (l.track != 0 ? tr.id != l.track : !NkTimelineModel::IsUnderObject(tr.object, l.object)) {
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
				if (tr.kind == NkTimelineValueKind::Clips || m.IsHidden(tr.object)) {
					return false;
				}
				return m.activeTrack != 0 ? tr.id == m.activeTrack : tr.showCurve;
			}

			/// Le point de courbe sous le curseur (piste, cle, canal).
			bool PointSous(Ctx &c, nk_uint64 &piste, int32 &cle, int32 &canal) {
				const float32 hit = c.M("hit_r");
				float32 meilleur = hit * hit;
				bool trouve = false;
				for (uint32 i = 0; i < (uint32)c.m.tracks.Size(); ++i) {
					const NkTimelineTrack &tr = c.m.tracks[i];
					if (!CourbeMontree(c.m, tr) || tr.locked) {
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

			/// Le bout d'une poignee de tangente a l'ecran (longueur fixe en pixels,
			/// direction de la pente). `sortie` : vers la droite.
			void BoutTangente(const Ctx &c, float32 t, float32 v, float32 pente, bool sortie, float32 &x, float32 &y) {
				const NkPaintRect &a = c.plan.area;
				const float32 x0 = NkTimelineTimeToX(c.m, a, t), y0 = NkTimelineValueToY(c.m, a, v);
				const float32 dt = (c.m.viewEnd - c.m.viewStart) * 0.1f;
				const float32 x1 = NkTimelineTimeToX(c.m, a, t + dt), y1 = NkTimelineValueToY(c.m, a, v + pente * dt);
				float32 dx = x1 - x0, dy = y1 - y0;
				const float32 n = std::sqrt(dx * dx + dy * dy);
				const float32 L = c.M("tangent_len");
				dx = n > 1e-4f ? dx / n * L : L;
				dy = n > 1e-4f ? dy / n * L : 0.f;
				x = sortie ? x0 + dx : x0 - dx;
				y = sortie ? y0 + dy : y0 - dy;
			}

			/// La poignee de tangente sous le curseur (cles choisies d'une courbe).
			bool TangenteSous(Ctx &c, nk_uint64 &piste, int32 &cle, int32 &canal, bool &sortie) {
				const float32 hit = c.M("hit_r");
				for (uint32 i = 0; i < (uint32)c.m.tracks.Size(); ++i) {
					const NkTimelineTrack &tr = c.m.tracks[i];
					if (!CourbeMontree(c.m, tr) || tr.locked) {
						continue;
					}
					for (uint32 k = 0; k < (uint32)tr.keys.Size(); ++k) {
						const NkTimelineKey &key = tr.keys[k];
						if (!key.selected) {
							continue;
						}
						for (uint32 ch = 0; ch < tr.channels; ++ch) {
							if (!tr.showChannel[ch]) {
								continue;
							}
							float32 pin, pout;
							c.m.KeySlopes(tr, k, ch, pin, pout);
							for (int32 cote = 0; cote < 2; ++cote) {
								const bool so = cote == 1;
								// La poignee n'existe que du cote d'un segment Courbe.
								const bool utile = so ? (k + 1 < (uint32)tr.keys.Size() && key.interp == (uint8)NkTimelineInterp::Courbe)
													  : (k > 0 && tr.keys[k - 1].interp == (uint8)NkTimelineInterp::Courbe);
								if (!utile) {
									continue;
								}
								float32 x, y;
								BoutTangente(c, key.time, key.v[ch], so ? pout : pin, so, x, y);
								if (Abs(x - c.in.mouseX) <= hit && Abs(y - c.in.mouseY) <= hit) {
									piste = tr.id;
									cle = (int32)k;
									canal = (int32)ch;
									sortie = so;
									return true;
								}
							}
						}
					}
				}
				return false;
			}

			/// La BOITE DE TRANSFORMATION des cles choisies (feuille d'images) :
			/// faux s'il y en a moins de deux ou si elles tiennent sur une image.
			bool Boite(const Ctx &c, NkPaintRect &b) {
				const NkTimelineModel &m = c.m;
				if (m.curveMode || m.SelectionCount() < 2) {
					return false;
				}
				float32 t0, t1;
				if (!m.SelectionTimeRange(t0, t1) || t1 - t0 < m.FrameDuration() * 0.5f) {
					return false;
				}
				float32 y0 = 1e30f, y1 = -1e30f;
				for (uint32 k = 0; k < (uint32)c.r.rows.Size(); ++k) {
					const NkTimelineRow &l = c.r.rows[k];
					const NkTimelineTrack *tr = m.Track(l.track);
					if (tr == nullptr) {
						continue;
					}
					for (uint32 q = 0; q < (uint32)tr->keys.Size(); ++q) {
						if (tr->keys[q].selected) {
							y0 = Min(y0, l.y);
							y1 = Max(y1, l.y + l.h);
							break;
						}
					}
				}
				if (y0 > y1) {
					return false;
				}
				const float32 marge = c.M("key_r") + c.M("button_gap") * 2.f;
				const float32 x0 = NkTimelineTimeToX(m, c.plan.area, t0) - marge;
				const float32 x1 = NkTimelineTimeToX(m, c.plan.area, t1) + marge;
				b = NkPaintRect{x0, y0 + c.M("button_gap"), x1 - x0, y1 - y0 - c.M("button_gap") * 2.f};
				return true;
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

			/// La boite tenue par un bord : les temps choisis repartent de leur
			/// origine, mis a l'echelle autour du bord oppose.
			void Etirer(Ctx &c) {
				NkTimelineModel &m = c.m;
				const float32 pivot = m.gestureA, bord = m.gestureB;
				const float32 span = bord - pivot;
				if (Abs(span) < 1e-6f) {
					return;
				}
				// Le bord SUIT la souris depuis l'appui (pas de saut a la prise).
				const float32 bordNeuf = bord + NkTimelineXToTime(m, c.plan.area, c.in.mouseX) - m.gestureT0;
				float32 f = (bordNeuf - pivot) / span;
				f = f < 0.f ? 0.f : f;
				uint32 n = 0;
				for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
					for (uint32 k = 0; k < (uint32)m.tracks[i].keys.Size(); ++k) {
						NkTimelineKey &key = m.tracks[i].keys[k];
						if (key.selected && n < (uint32)m.gestureTimes.Size()) {
							const float32 t = m.Snap(pivot + (m.gestureTimes[n] - pivot) * f);
							key.time = t < 0.f ? 0.f : t;
							++n;
						}
					}
				}
				m.Touch();
				c.r.changed = true;
			}

			void BoiteChoisir(Ctx &c, bool additive) {
				NkTimelineModel &m = c.m;
				const NkPaintRect &area = c.plan.area;
				const float32 x0 = Min(m.gestureX0, c.in.mouseX), x1 = Max(m.gestureX0, c.in.mouseX);
				const float32 y0 = Min(m.gestureY0, c.in.mouseY), y1 = Max(m.gestureY0, c.in.mouseY);
				if (!additive) {
					m.ClearSelection();
				}
				const float32 t0 = NkTimelineXToTime(m, area, x0);
				const float32 t1 = NkTimelineXToTime(m, area, x1);
				if (m.curveMode) {
					for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
						NkTimelineTrack &tr = m.tracks[i];
						if (!CourbeMontree(m, tr) || tr.locked) {
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
						NkTimelineTrack &tr = m.tracks[i];
						const bool dedans =
							l.track != 0 ? tr.id == l.track : NkTimelineModel::IsUnderObject(tr.object, l.object);
						if (!dedans || tr.locked) {
							continue;
						}
						m.SelectInBox((int32)i, (int32)i, t0, t1, true);
						// Les clips que le cadre touche se choisissent aussi.
						for (uint32 q = 0; q < (uint32)tr.clips.Size(); ++q) {
							if (tr.clips[q].End() >= t0 && tr.clips[q].start <= t1) {
								tr.clips[q].selected = true;
							}
						}
					}
				}
			}

			/// Le clip sous le curseur dans une ligne de clips, et la partie tenue :
			/// 0 corps, 1 bord gauche, 2 bord droit, 3 fondu d'entree, 4 de sortie.
			nk_uint64 ClipSous(Ctx &c, const NkTimelineRow &l, int32 &partie) {
				const NkTimelineTrack *tr = c.m.Track(l.track);
				if (tr == nullptr || !EstClips(*tr) || tr->locked) {
					return 0;
				}
				const float32 e = c.M("edge_px");
				const NkPaintRect &a = c.plan.area;
				const float32 haut = l.y + c.M("button_gap") * 2.f;
				for (int32 k = (int32)tr->clips.Size() - 1; k >= 0; --k) {
					const NkTimelineClip &cl = tr->clips[(uint32)k];
					const float32 x0 = NkTimelineTimeToX(c.m, a, cl.start), x1 = NkTimelineTimeToX(c.m, a, cl.End());
					if (c.in.mouseX < x0 - e || c.in.mouseX > x1 + e) {
						continue;
					}
					// Les poignees de fondu : les coins hauts, a l'interieur.
					const float32 xi = NkTimelineTimeToX(c.m, a, cl.start + cl.blendIn);
					const float32 xo = NkTimelineTimeToX(c.m, a, cl.End() - cl.blendOut);
					if (Abs(c.in.mouseY - haut) <= e) {
						if (Abs(c.in.mouseX - xi) <= e && c.in.mouseX > x0 + e * 0.5f) {
							partie = 3;
							return cl.id;
						}
						if (Abs(c.in.mouseX - xo) <= e && c.in.mouseX < x1 - e * 0.5f) {
							partie = 4;
							return cl.id;
						}
					}
					if (Abs(c.in.mouseX - x0) <= e) {
						partie = 1;
					} else if (Abs(c.in.mouseX - x1) <= e) {
						partie = 2;
					} else {
						partie = 0;
					}
					return cl.id;
				}
				return 0;
			}

			/// Le marqueur sous le curseur dans l'etage haut de la regle.
			nk_uint64 MarqueurSous(const Ctx &c) {
				const NkPaintRect &rg = c.plan.ruler;
				if (c.in.mouseY > rg.y + rg.h * 0.5f) {
					return 0;
				}
				const float32 hit = c.M("hit_r");
				for (int32 k = (int32)c.m.markers.Size() - 1; k >= 0; --k) {
					const float32 x = NkTimelineTimeToX(c.m, c.plan.area, c.m.markers[(uint32)k].time);
					if (c.in.mouseX >= x - hit && c.in.mouseX <= x + hit + c.M("marker_h")) {
						return c.m.markers[(uint32)k].id;
					}
				}
				return 0;
			}

			/// La geometrie de la barre de defilement : l'etendue [lo, hi] et le pouce.
			void Defilement(const Ctx &c, float32 &lo, float32 &hi, NkPaintRect &pouce) {
				const NkTimelineModel &m = c.m;
				const float32 d = m.duration > 1e-3f ? m.duration : 1.f;
				lo = Min(m.viewStart, -d * 0.05f);
				hi = Max(m.viewEnd, d * 1.1f);
				const NkPaintRect &sc = c.plan.scroll;
				const float32 k = sc.w / (hi - lo > 1e-6f ? hi - lo : 1e-6f);
				const float32 x0 = sc.x + (m.viewStart - lo) * k;
				const float32 x1 = sc.x + (m.viewEnd - lo) * k;
				pouce = NkPaintRect{x0, sc.y, Max(x1 - x0, c.M("edge_px") * 3.f), sc.h};
			}

			void ActionBouton(Ctx &c, uint8 b) {
				NkTimelineModel &m = c.m;
				switch ((NkTimelineButton)b) {
					case NkTimelineButton::First:
						m.SetCursor(m.PlayStart());
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
						m.SetCursor(m.PlayEnd());
						c.r.cursorChanged = true;
						break;
					case NkTimelineButton::Loop:
						m.loop = !m.loop;
						break;
					case NkTimelineButton::PrevFrame:
						m.StepFrame(-1);
						c.r.cursorChanged = true;
						break;
					case NkTimelineButton::NextFrame:
						m.StepFrame(1);
						c.r.cursorChanged = true;
						break;
					case NkTimelineButton::Speed: {
						static const float32 kVit[4] = {0.25f, 0.5f, 1.f, 2.f};
						int32 i = 0;
						while (i < 4 && kVit[i] <= m.speed + 1e-3f) {
							++i;
						}
						m.speed = kVit[i % 4];
						break;
					}
					case NkTimelineButton::Mode:
						m.curveMode = !m.curveMode;
						m.curveAutoFit = true;
						break;
					case NkTimelineButton::Snap:
						m.snap = !m.snap;
						break;
					case NkTimelineButton::FrameAll:
						m.FrameAll();
						m.viewGlide = false;
						break;
					case NkTimelineButton::AddKey: {
						const NkTimelineTrack *tr = m.Track(m.activeTrack);
						if (tr == nullptr && !m.tracks.Empty()) {
							tr = &m.tracks[0];
						}
						if (m.hostKeys) {
							c.r.keyRequested = true;
							c.r.requestTrack = tr != nullptr ? tr->id : 0;
							c.r.requestTime = m.cursor;
						} else if (tr != nullptr && !EstClips(*tr) && !tr->locked) {
							float32 v[4];
							c.ValueToKey(*tr, m.cursor, v);
							m.SetKey(tr->id, m.cursor, v);
							c.r.changed = true;
						}
						break;
					}
					case NkTimelineButton::Delete:
						if (m.hostKeys) {
							c.r.deleteRequested = true;
						} else {
							c.r.changed = m.DeleteSelection() || c.r.changed;
						}
						break;
					case NkTimelineButton::Copy:
						m.CopySelection();
						c.r.copyRequested = true;
						break;
					case NkTimelineButton::Paste:
						if (m.hostKeys) {
							c.r.pasteRequested = true;
							c.r.requestTime = m.cursor;
						} else {
							c.r.changed = m.PasteAt(m.cursor) || c.r.changed;
						}
						break;
					case NkTimelineButton::AddMarker:
						m.AddMarker(m.cursor);
						c.r.markersChanged = true;
						break;
					case NkTimelineButton::Undo:
						if (m.hostKeys) {
							c.r.undoRequested = true;
						} else {
							c.r.undone = m.Undo();
							c.r.changed = c.r.changed || c.r.undone;
						}
						break;
					case NkTimelineButton::Redo:
						if (m.hostKeys) {
							c.r.redoRequested = true;
						} else {
							c.r.redone = m.Redo();
							c.r.changed = c.r.changed || c.r.redone;
						}
						break;
					case NkTimelineButton::AddTrack:
						c.r.addTrackRequested = true;
						break;
					default:
						if (b >= (uint8)NkTimelineButton::InterpFirst && b < (uint8)NkTimelineButton::Count) {
							const uint32 avant = m.revision;
							m.SetSelectionInterp((uint8)(b - (uint8)NkTimelineButton::InterpFirst));
							c.r.changed = c.r.changed || m.revision != avant;
						} else if (b >= (uint8)NkTimelineButton::TangentFirst && b < (uint8)NkTimelineButton::InterpFirst) {
							const uint32 avant = m.revision;
							m.SetSelectionTangent((uint8)(b - (uint8)NkTimelineButton::TangentFirst));
							c.r.changed = c.r.changed || m.revision != avant;
						}
						break;
				}
			}

			/// Pose une cle (ou la DEMANDE a l'hote qui tient les cles).
			void PoserCle(Ctx &c, NkTimelineTrack &tr, float32 t, bool depuisCourbe) {
				NkTimelineModel &m = c.m;
				if (m.hostKeys) {
					c.r.keyRequested = true;
					c.r.requestTrack = tr.id;
					c.r.requestTime = t;
					return;
				}
				if (tr.locked || EstClips(tr)) {
					return;
				}
				float32 v[4];
				if (!depuisCourbe || !c.Eval(tr, t, v)) {
					c.ValueToKey(tr, t, v);
				}
				const int32 k = m.SetKey(tr.id, t, v);
				if (depuisCourbe) {
					m.SelectKey(tr.id, k, false);
				}
				c.r.changed = true;
			}

			/// Un appui dans la COLONNE (arbre, boutons de piste, valeurs).
			void AppuiColonne(Ctx &c) {
				NkTimelineModel &m = c.m;
				const NkTimelineRow *l = LigneSous(c, c.in.mouseY);
				if (l == nullptr) {
					return;
				}
				NkTimelineTrack *tr = m.Track(l->track);
				const ZonesLigne z = Zones(c, *l, tr);
				// Muet / solo / verrou : d'une piste, ou de tout un objet.
				const NkPaintRect *drap[3] = {&z.muet, &z.solo, &z.verrou};
				for (uint8 f = 0; f < 3; ++f) {
					if (!c.Over(*drap[f])) {
						continue;
					}
					if (tr != nullptr) {
						if (f == 0) {
							m.ToggleMute(tr->id);
						} else if (f == 1) {
							m.ToggleSolo(tr->id);
						} else {
							m.ToggleLock(tr->id);
						}
					} else {
						m.SetObjectFlag(l->object, f, !m.ObjectFlag(l->object, f));
					}
					c.r.flagsChanged = true;
					c.r.changed = true;
					return;
				}
				if (l->track == 0) {
					m.ToggleCollapsed(l->object);
					return;
				}
				if (tr == nullptr) {
					return;
				}
				if (c.Over(z.prec)) {
					float32 t = -1e30f;
					for (uint32 k = 0; k < (uint32)tr->keys.Size(); ++k) {
						if (tr->keys[k].time < m.cursor - m.FrameDuration() * 0.5f) {
							t = tr->keys[k].time;
						}
					}
					if (t > -1e29f) {
						m.SetCursor(t);
						c.r.cursorChanged = true;
					}
					return;
				}
				if (c.Over(z.suiv)) {
					for (uint32 k = 0; k < (uint32)tr->keys.Size(); ++k) {
						if (tr->keys[k].time > m.cursor + m.FrameDuration() * 0.5f) {
							m.SetCursor(tr->keys[k].time);
							c.r.cursorChanged = true;
							break;
						}
					}
					return;
				}
				if (c.Over(z.cle)) {
					if (EstClips(*tr)) {
						c.r.addClipRequested = true;
						c.r.requestTrack = tr->id;
						c.r.requestTime = m.cursor;
					} else {
						PoserCle(c, *tr, m.cursor, false);
					}
					return;
				}
				if (c.Over(z.retirer)) {
					m.RemoveTrack(tr->id);
					c.r.trackRemoved = true;
					c.r.changed = true;
					return;
				}
				for (uint32 ch = 0; ch < 4; ++ch) {
					if (m.curveMode && ch < tr->channels && c.Over(z.bascule[ch])) {
						tr->showChannel[ch] = !tr->showChannel[ch];
						return;
					}
					if (c.Over(z.canal[ch]) && !tr->locked) {
						float32 v[4];
						c.ValueAtCursor(*tr, v);
						m.gesture = (uint8)NkTimelineGesture::ScrubValue;
						m.gestureTrack = tr->id;
						m.gestureChannel = (int32)ch;
						m.gestureV0 = EstClips(*tr) ? tr->weight : v[ch];
						m.activeTrack = tr->id;
						return;
					}
				}
				// Un clic sur la piste ACTIVE la rend a toutes (courbes : on revoit tout).
				m.activeTrack = m.activeTrack == tr->id ? 0 : tr->id;
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
				m.gestureClip = 0;
				m.gestureMarker = 0;
				// Les deux barres et « + Piste ».
				for (uint8 b = 0; b < (uint8)NkTimelineButton::Count; ++b) {
					if (c.Over(c.r.buttons[b])) {
						ActionBouton(c, b);
						return;
					}
				}
				const Plan &pl = c.plan;
				// La barre de defilement : le pouce glisse la vue, ses bords zooment.
				if (c.Over(pl.scroll)) {
					float32 lo, hi;
					NkPaintRect pouce;
					Defilement(c, lo, hi, pouce);
					const float32 e = c.M("edge_px");
					m.gestureViewStart = m.viewStart;
					m.gestureViewEnd = m.viewEnd;
					m.gestureA = lo;
					m.gestureB = hi;
					m.viewGlide = false;
					if (Abs(in.mouseX - pouce.x) <= e) {
						m.gesture = (uint8)NkTimelineGesture::ScrollEdgeL;
					} else if (Abs(in.mouseX - (pouce.x + pouce.w)) <= e) {
						m.gesture = (uint8)NkTimelineGesture::ScrollEdgeR;
					} else if (c.Over(pouce)) {
						m.gesture = (uint8)NkTimelineGesture::ScrollThumb;
					} else {
						// Hors du pouce : la vue se centre la.
						const float32 t = lo + (in.mouseX - pl.scroll.x) / (pl.scroll.w > 1.f ? pl.scroll.w : 1.f) * (hi - lo);
						const float32 span = m.viewEnd - m.viewStart;
						m.viewStart = t - span * 0.5f;
						m.viewEnd = t + span * 0.5f;
					}
					return;
				}
				if (c.Over(pl.ruler)) {
					// L'etage HAUT : les poignees de plage et les marqueurs d'abord.
					const bool haut = in.mouseY < pl.ruler.y + pl.ruler.h * 0.5f;
					if (haut) {
						const float32 rw = c.M("range_w");
						const float32 xs = NkTimelineTimeToX(m, pl.area, m.PlayStart());
						const float32 xe = NkTimelineTimeToX(m, pl.area, m.PlayEnd());
						if (in.mouseX >= xs - rw && in.mouseX <= xs + rw * 0.5f) {
							m.gesture = (uint8)NkTimelineGesture::RangeStart;
							return;
						}
						if (in.mouseX >= xe - rw * 0.5f && in.mouseX <= xe + rw) {
							m.gesture = (uint8)NkTimelineGesture::RangeEnd;
							return;
						}
						const nk_uint64 mk = MarqueurSous(c);
						if (mk != 0) {
							if (!ajout) {
								m.ClearSelection();
							}
							if (NkTimelineMarker *k = m.Marker(mk)) {
								k->selected = true;
								m.gestureA = k->time;
							}
							m.gesture = (uint8)NkTimelineGesture::MoveMarker;
							m.gestureMarker = mk;
							m.gestureT0 = NkTimelineXToTime(m, pl.area, in.mouseX);
							return;
						}
					}
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
						m.viewGlide = false;
						return;
					}
					nk_uint64 piste = 0;
					int32 cle = -1, canal = -1;
					if (m.curveMode) {
						bool sortie = true;
						if (TangenteSous(c, piste, cle, canal, sortie)) {
							m.gesture = (uint8)NkTimelineGesture::MoveTangent;
							m.gestureTrack = piste;
							m.gestureKey = cle;
							m.gestureChannel = canal;
							m.gestureTangentOut = sortie;
							return;
						}
						PointSous(c, piste, cle, canal);
					} else {
						// La boite de transformation : ses bords mettent a l'echelle.
						NkPaintRect b;
						if (Boite(c, b) && in.mouseY >= b.y && in.mouseY <= b.y + b.h) {
							const float32 e = c.M("edge_px");
							float32 t0, t1;
							m.SelectionTimeRange(t0, t1);
							const bool gauche = Abs(in.mouseX - b.x) <= e;
							const bool droite = Abs(in.mouseX - (b.x + b.w)) <= e;
							if (gauche || droite) {
								m.gesture = (uint8)NkTimelineGesture::ScaleKeys;
								m.gestureA = gauche ? t1 : t0; // le pivot : le bord oppose
								m.gestureB = gauche ? t0 : t1; // le bord tenu
								m.gestureT0 = NkTimelineXToTime(m, pl.area, in.mouseX);
								RetenirChoisies(c);
								return;
							}
						}
						if (const NkTimelineRow *l = LigneSous(c, in.mouseY)) {
							// Une piste de CLIPS : un clip, ses bords, ses fondus.
							int32 partie = 0;
							const nk_uint64 cl = ClipSous(c, *l, partie);
							if (cl != 0) {
								NkTimelineClip *k = m.Clip(l->track, cl);
								if (!k->selected || in.ctrl) {
									m.SelectClip(cl, ajout);
								}
								m.activeClip = cl;
								m.activeTrack = l->track;
								m.gestureClip = cl;
								m.gestureTrack = l->track;
								m.gestureT0 = NkTimelineXToTime(m, pl.area, in.mouseX);
								m.gestureA = k->start;
								m.gestureB = k->length;
								m.gestureC = partie == 3 ? k->blendIn : (partie == 4 ? k->blendOut : k->offset);
								static const NkTimelineGesture kG[5] = {
									NkTimelineGesture::MoveClip, NkTimelineGesture::TrimClipStart,
									NkTimelineGesture::TrimClipEnd, NkTimelineGesture::ClipBlendIn,
									NkTimelineGesture::ClipBlendOut};
								m.gesture = (uint8)kG[partie];
								// Les clips choisis bougent ensemble : leurs debuts d'origine.
								m.gestureTimes.Clear();
								for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
									for (uint32 q = 0; q < (uint32)m.tracks[i].clips.Size(); ++q) {
										if (m.tracks[i].clips[q].selected) {
											m.gestureTimes.PushBack(m.tracks[i].clips[q].start);
										}
									}
								}
								return;
							}
							cle = CleSous(c, *l, piste);
							// Un losange de RESUME (en-tete d'objet) prend toutes les cles
							// de l'objet (et de ses descendants) a ce temps.
							if (cle >= 0 && l->track == 0) {
								const float32 t = m.Track(piste)->keys[(uint32)cle].time;
								if (!ajout) {
									m.ClearSelection();
								}
								for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
									NkTimelineTrack &tr = m.tracks[i];
									if (!tr.locked && NkTimelineModel::IsUnderObject(tr.object, l->object)) {
										const int32 k = m.KeyAt(tr, t);
										if (k >= 0) {
											tr.keys[(uint32)k].selected = true;
										}
									}
								}
								m.gesture = (uint8)NkTimelineGesture::MoveKeys;
								m.gestureKey = -1;
								m.gestureT0 = NkTimelineXToTime(m, pl.area, in.mouseX);
								RetenirChoisies(c);
								return;
							}
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
					AppuiColonne(c);
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
				auto instantane = [&]() {
					if (!m.gestureSnapshot) {
						m.PushUndo();
						m.gestureSnapshot = true;
					}
				};
				const float32 tSouris = NkTimelineXToTime(m, pl.area, in.mouseX);
				switch ((NkTimelineGesture)m.gesture) {
					case NkTimelineGesture::Scrub: {
						const float32 avant = m.cursor;
						m.SetCursor(m.Snap(tSouris));
						c.r.cursorChanged = c.r.cursorChanged || m.cursor != avant;
						break;
					}
					case NkTimelineGesture::MoveKeys:
						if (m.gestureMoved) {
							instantane();
							Glisser(c);
						}
						break;
					case NkTimelineGesture::ScaleKeys:
						if (m.gestureMoved) {
							instantane();
							Etirer(c);
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
							instantane();
							if (EstClips(*tr)) {
								// L'influence d'une piste de clips, dans [0, 1].
								float32 w = m.gestureV0 + dx * c.Raw("scrub_color_px");
								tr->weight = w < 0.f ? 0.f : (w > 1.f ? 1.f : w);
								m.Touch();
								c.r.clipsChanged = true;
								c.r.changed = true;
								break;
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
					case NkTimelineGesture::MoveClip:
						if (m.gestureMoved) {
							instantane();
							const float32 dt = tSouris - m.gestureT0;
							uint32 n = 0;
							for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
								for (uint32 q = 0; q < (uint32)m.tracks[i].clips.Size(); ++q) {
									NkTimelineClip &cl = m.tracks[i].clips[q];
									if (cl.selected && n < (uint32)m.gestureTimes.Size()) {
										const float32 t = m.Snap(m.gestureTimes[n] + dt);
										cl.start = t < 0.f ? 0.f : t;
										m.duration = cl.End() > m.duration ? cl.End() : m.duration;
										++n;
									}
								}
							}
							m.Touch();
							c.r.clipsChanged = true;
							c.r.changed = true;
						}
						break;
					case NkTimelineGesture::TrimClipStart:
					case NkTimelineGesture::TrimClipEnd:
					case NkTimelineGesture::ClipBlendIn:
					case NkTimelineGesture::ClipBlendOut: {
						NkTimelineClip *cl = m.Clip(m.gestureTrack, m.gestureClip);
						if (cl == nullptr || !m.gestureMoved) {
							break;
						}
						instantane();
						const float32 f = m.FrameDuration();
						const float32 dt = m.Snap(tSouris) - m.Snap(m.gestureT0);
						const float32 fin0 = m.gestureA + m.gestureB;
						if (m.gesture == (uint8)NkTimelineGesture::TrimClipStart) {
							// Rogner a gauche : le debut avance, le temps du clip suit.
							float32 debut = m.gestureA + dt;
							debut = debut < 0.f ? 0.f : (debut > fin0 - f ? fin0 - f : debut);
							cl->start = debut;
							cl->length = fin0 - debut;
							cl->offset = m.gestureC + (debut - m.gestureA) * cl->rate;
						} else if (m.gesture == (uint8)NkTimelineGesture::TrimClipEnd) {
							const float32 l = m.gestureB + dt;
							cl->length = l < f ? f : l;
							m.duration = cl->End() > m.duration ? cl->End() : m.duration;
						} else if (m.gesture == (uint8)NkTimelineGesture::ClipBlendIn) {
							const float32 b = m.gestureC + dt;
							cl->blendIn = b < 0.f ? 0.f : (b > cl->length - cl->blendOut ? cl->length - cl->blendOut : b);
						} else {
							const float32 b = m.gestureC - dt;
							cl->blendOut = b < 0.f ? 0.f : (b > cl->length - cl->blendIn ? cl->length - cl->blendIn : b);
						}
						m.Touch();
						c.r.clipsChanged = true;
						c.r.changed = true;
						break;
					}
					case NkTimelineGesture::MoveMarker:
						if (m.gestureMoved) {
							if (NkTimelineMarker *k = m.Marker(m.gestureMarker)) {
								instantane();
								const float32 t = m.Snap(m.gestureA + tSouris - m.gestureT0);
								k->time = t < 0.f ? 0.f : t;
								m.Touch();
								c.r.markersChanged = true;
							}
						}
						break;
					case NkTimelineGesture::RangeStart:
					case NkTimelineGesture::RangeEnd: {
						if (!m.gestureMoved) {
							break;
						}
						instantane();
						const float32 t = m.Snap(tSouris < 0.f ? 0.f : (tSouris > m.duration ? m.duration : tSouris));
						const float32 f = m.FrameDuration();
						if (m.gesture == (uint8)NkTimelineGesture::RangeStart) {
							const float32 fin = m.PlayEnd();
							m.rangeEnd = fin;
							m.rangeStart = t > fin - f ? fin - f : t;
						} else {
							const float32 debut = m.PlayStart();
							m.rangeStart = debut;
							m.rangeEnd = t < debut + f ? debut + f : t;
						}
						m.Touch();
						c.r.markersChanged = true;
						break;
					}
					case NkTimelineGesture::ScrollThumb:
					case NkTimelineGesture::ScrollEdgeL:
					case NkTimelineGesture::ScrollEdgeR: {
						const float32 k = (m.gestureB - m.gestureA) / (pl.scroll.w > 1.f ? pl.scroll.w : 1.f);
						const float32 dtv = dx * k;
						const float32 minSpan = m.FrameDuration() * 4.f;
						if (m.gesture == (uint8)NkTimelineGesture::ScrollThumb) {
							m.viewStart = m.gestureViewStart + dtv;
							m.viewEnd = m.gestureViewEnd + dtv;
						} else if (m.gesture == (uint8)NkTimelineGesture::ScrollEdgeL) {
							const float32 a = m.gestureViewStart + dtv;
							m.viewStart = a < m.viewEnd - minSpan ? a : m.viewEnd - minSpan;
						} else {
							const float32 b = m.gestureViewEnd + dtv;
							m.viewEnd = b > m.viewStart + minSpan ? b : m.viewStart + minSpan;
						}
						break;
					}
					case NkTimelineGesture::MoveTangent: {
						NkTimelineTrack *tr = m.Track(m.gestureTrack);
						if (tr == nullptr || m.gestureKey < 0 || m.gestureKey >= (int32)tr->keys.Size() || m.gestureChannel < 0) {
							break;
						}
						instantane();
						NkTimelineKey &key = tr->keys[(uint32)m.gestureKey];
						const uint32 ch = (uint32)m.gestureChannel;
						// La pente que montre le curseur (jamais verticale).
						float32 dtk = tSouris - key.time;
						const float32 mini = m.FrameDuration() * 0.05f;
						if (m.gestureTangentOut) {
							dtk = dtk < mini ? mini : dtk;
						} else {
							dtk = dtk > -mini ? -mini : dtk;
						}
						const float32 pente = (NkTimelineYToValue(m, pl.area, in.mouseY) - key.v[ch]) / dtk;
						if (key.tangent == (uint8)NkTimelineTangent::Auto || key.tangent == (uint8)NkTimelineTangent::Plate) {
							// Toucher une poignee la rend « a la main », sans saut.
							for (uint32 q = 0; q < 4; ++q) {
								float32 a, b;
								m.KeySlopes(*tr, (uint32)m.gestureKey, q, a, b);
								key.tanIn[q] = a;
								key.tanOut[q] = b;
							}
							key.tangent = in.shift ? (uint8)NkTimelineTangent::Cassee : (uint8)NkTimelineTangent::Unifiee;
						}
						if (key.tangent == (uint8)NkTimelineTangent::Cassee) {
							(m.gestureTangentOut ? key.tanOut : key.tanIn)[ch] = pente;
						} else {
							key.tanIn[ch] = key.tanOut[ch] = pente;
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
				NkTimelineModel &m = c.m;
				switch ((NkTimelineGesture)m.gesture) {
					case NkTimelineGesture::MoveKeys:
					case NkTimelineGesture::ScaleKeys:
						if (m.gestureMoved) {
							m.Normalize();
							c.r.changed = true;
						} else if (m.gesture == (uint8)NkTimelineGesture::MoveKeys && !c.in.ctrl && !c.in.shift &&
								   m.gestureKey >= 0) {
							// Un CLIC sur une cle deja choisie (sans glisser) la choisit seule.
							m.SelectKey(m.gestureTrack, m.gestureKey, false);
						}
						break;
					case NkTimelineGesture::MoveClip:
						if (m.gestureMoved) {
							m.SortClips();
						} else if (!c.in.ctrl && !c.in.shift) {
							m.SelectClip(m.gestureClip, false);
						}
						break;
					case NkTimelineGesture::MoveMarker:
						if (m.gestureMoved) {
							// Les marqueurs restent tries par temps.
							for (uint32 i = 1; i < (uint32)m.markers.Size(); ++i) {
								NkTimelineMarker k = m.markers[i];
								uint32 j = i;
								while (j > 0 && m.markers[j - 1].time > k.time) {
									m.markers[j] = m.markers[j - 1];
									--j;
								}
								m.markers[j] = k;
							}
						} else {
							// Un clic sur un marqueur y pose le curseur.
							if (const NkTimelineMarker *k = m.Marker(m.gestureMarker)) {
								m.SetCursor(k->time);
								c.r.cursorChanged = true;
							}
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
				if (EstClips(*tr)) {
					if (m.ClipAt(*tr, t) == 0) {
						c.r.addClipRequested = true;
						c.r.requestTrack = tr->id;
						c.r.requestTime = t;
					}
				} else {
					PoserCle(c, *tr, t, true);
				}
				// Le second appui du double-clic a ouvert un cadre : il ne doit pas,
				// au relache, defaire la selection de la cle qu'on vient de poser.
				m.gesture = (uint8)NkTimelineGesture::None;
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
				if (c.Over(pl.scroll) || (zone && in.ctrl && !m.curveMode)) {
					// Defilement horizontal ADOUCI : un dixieme de la vue par cran.
					const float32 span = m.viewEnd - m.viewStart;
					if (!m.viewGlide) {
						m.viewTargetStart = m.viewStart;
						m.viewTargetEnd = m.viewEnd;
					}
					const float32 d = (in.wheel > 0.f ? -0.1f : 0.1f) * span;
					m.viewTargetStart += d;
					m.viewTargetEnd += d;
					m.viewGlide = true;
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
				m.ZoomTimeSmooth(NkTimelineXToTime(m, pl.area, in.mouseX), f);
			}

			// ── 3. LE DESSIN ────────────────────────────────────────────────────
			void FondBouton(Ctx &c, const NkPaintRect &rc, bool enfonce, bool actif, int32 indice) {
				const bool survol = c.Over(rc);
				if (survol && indice >= 0) {
					c.r.hoveredButton = indice;
				}
				const float32 rd = c.M("round");
				if (enfonce) {
					c.p.Fill(rc, c.s.accent, rd);
				} else if (survol && actif) {
					c.p.Fill(rc, c.s.inputBg, rd);
				}
			}

			void Bouton(Ctx &c, NkTimelineButton b, const char *texte, bool enfonce, bool actif) {
				const NkPaintRect &rc = c.r.buttons[(uint8)b];
				if (rc.w <= 0.f) {
					return;
				}
				FondBouton(c, rc, enfonce, actif, (int32)b);
				if (texte != nullptr) {
					c.p.Text(rc, texte, enfonce ? c.s.textOnAccent : (actif ? c.s.text : c.s.textMuted), NkTextAlign::Center);
				}
			}

			/// Un trait epais (icones).
			void Trait(Ctx &c, float32 x0, float32 y0, float32 x1, float32 y1, uint16 role, float32 ep) {
				c.p.Line(x0, y0, x1, y1, role, ep);
			}

			/// Les ICONES des barres, tracees (aucune police d'icones requise).
			void Icone(Ctx &c, NkTimelineButton b, uint16 role) {
				const NkPaintRect &rc = c.r.buttons[(uint8)b];
				if (rc.w <= 0.f) {
					return;
				}
				const float32 cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f;
				const float32 d = rc.h * 0.26f;
				const float32 ep = c.M("stroke_w") * 1.5f;
				const NkTimelineModel &m = c.m;
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
						Triangle(c, cx + d * 0.1f, cy - d, cx + d * 0.1f, cy + d, cx - d, cy, role);
						Losange(c, cx + d * 0.75f, cy, d * 0.55f, c.s.keySelected);
						break;
					case NkTimelineButton::NextKey:
						Losange(c, cx - d * 0.75f, cy, d * 0.55f, c.s.keySelected);
						Triangle(c, cx - d * 0.1f, cy - d, cx - d * 0.1f, cy + d, cx + d, cy, role);
						break;
					case NkTimelineButton::PrevFrame:
						c.p.Fill(NkPaintRect{cx - d, cy - d, ep, d * 2.f}, role);
						Triangle(c, cx + d * 0.6f, cy - d * 0.8f, cx + d * 0.6f, cy + d * 0.8f, cx - d * 0.5f, cy, role);
						break;
					case NkTimelineButton::NextFrame:
						c.p.Fill(NkPaintRect{cx + d - ep, cy - d, ep, d * 2.f}, role);
						Triangle(c, cx - d * 0.6f, cy - d * 0.8f, cx - d * 0.6f, cy + d * 0.8f, cx + d * 0.5f, cy, role);
						break;
					case NkTimelineButton::Play:
						if (m.playing) {
							c.p.Fill(NkPaintRect{cx - d * 0.8f, cy - d, d * 0.6f, d * 2.f}, role);
							c.p.Fill(NkPaintRect{cx + d * 0.2f, cy - d, d * 0.6f, d * 2.f}, role);
						} else {
							Triangle(c, cx - d * 0.7f, cy - d * 1.1f, cx - d * 0.7f, cy + d * 1.1f, cx + d * 1.1f, cy, role);
						}
						break;
					case NkTimelineButton::Loop: {
						// Une boucle : un cadre arrondi ouvert et sa pointe.
						Trait(c, cx - d, cy - d * 0.5f, cx + d * 0.6f, cy - d * 0.5f, role, ep);
						Trait(c, cx + d, cy - d * 0.1f, cx + d, cy + d * 0.5f, role, ep);
						Trait(c, cx + d, cy + d * 0.5f, cx - d * 0.6f, cy + d * 0.5f, role, ep);
						Trait(c, cx - d, cy + d * 0.1f, cx - d, cy - d * 0.5f, role, ep);
						Triangle(c, cx + d * 0.3f, cy - d, cx + d * 0.3f, cy, cx + d, cy - d * 0.5f, role);
						Triangle(c, cx - d * 0.3f, cy, cx - d * 0.3f, cy + d, cx - d, cy + d * 0.5f, role);
						break;
					}
					case NkTimelineButton::AddKey: {
						Losange(c, cx - d * 0.3f, cy + d * 0.15f, d * 0.85f, role);
						const float32 px = cx + d * 0.75f, py = cy - d * 0.55f, q = d * 0.45f;
						Trait(c, px - q, py, px + q, py, c.s.keySelected, ep);
						Trait(c, px, py - q, px, py + q, c.s.keySelected, ep);
						break;
					}
					case NkTimelineButton::Delete:
						Trait(c, cx - d * 0.8f, cy - d * 0.8f, cx + d * 0.8f, cy + d * 0.8f, role, ep * 1.3f);
						Trait(c, cx + d * 0.8f, cy - d * 0.8f, cx - d * 0.8f, cy + d * 0.8f, role, ep * 1.3f);
						break;
					case NkTimelineButton::Copy: {
						const float32 q = d * 0.75f;
						c.p.Outline(NkPaintRect{cx - d, cy - d, q * 1.8f, q * 1.8f}, role, c.s.headerBg, c.M("stroke_w"));
						c.p.Outline(NkPaintRect{cx - d + q * 0.7f, cy - d + q * 0.7f, q * 1.8f, q * 1.8f}, role, c.s.headerBg,
									c.M("stroke_w"));
						break;
					}
					case NkTimelineButton::Paste: {
						c.p.Outline(NkPaintRect{cx - d * 0.8f, cy - d * 0.8f, d * 1.6f, d * 1.9f}, role, c.s.headerBg,
									c.M("stroke_w"));
						c.p.Fill(NkPaintRect{cx - d * 0.4f, cy - d * 1.05f, d * 0.8f, d * 0.45f}, role);
						Trait(c, cx - d * 0.4f, cy, cx + d * 0.4f, cy, role, c.M("stroke_w"));
						Trait(c, cx - d * 0.4f, cy + d * 0.45f, cx + d * 0.4f, cy + d * 0.45f, role, c.M("stroke_w"));
						break;
					}
					case NkTimelineButton::AddMarker: {
						const uint16 mr = c.Role(c.s.marker, c.s.keySelected);
						Trait(c, cx - d * 0.6f, cy - d, cx - d * 0.6f, cy + d, role, ep);
						Triangle(c, cx - d * 0.6f, cy - d, cx + d, cy - d * 0.45f, cx - d * 0.6f, cy + d * 0.1f, mr);
						break;
					}
					case NkTimelineButton::Undo:
					case NkTimelineButton::Redo: {
						// Une fleche courbe : un arc de cinq segments et sa pointe.
						const float32 sens = b == NkTimelineButton::Undo ? 1.f : -1.f;
						float32 px = 0.f, py = 0.f;
						for (int32 i = 0; i <= 6; ++i) {
							const float32 a = 3.14159f * (0.15f + 0.85f * (float32)i / 6.f);
							const float32 x = cx + std::cos(a) * d * sens * -1.f, y = cy + d * 0.3f - std::sin(a) * d;
							if (i > 0) {
								Trait(c, px, py, x, y, role, ep);
							}
							px = x;
							py = y;
						}
						const float32 tx = cx - d * sens, ty = cy + d * 0.3f;
						Triangle(c, tx - d * 0.45f, ty - d * 0.1f, tx + d * 0.45f, ty - d * 0.1f, tx, ty + d * 0.5f, role);
						break;
					}
					default: {
						// Les tangentes : une cle et ses deux poignees.
						if (b >= NkTimelineButton::TangentFirst && b < NkTimelineButton::InterpFirst) {
							const uint8 t = (uint8)b - (uint8)NkTimelineButton::TangentFirst;
							const uint16 tr = c.Role(c.s.tangent, c.s.keySelected);
							const float32 L = d * 1.1f;
							float32 ya = 0.f, yb = 0.f;
							if (t == (uint8)NkTimelineTangent::Unifiee) {
								ya = d * 0.6f;
								yb = -d * 0.6f;
							} else if (t == (uint8)NkTimelineTangent::Cassee) {
								ya = -d * 0.7f;
								yb = -d * 0.7f;
							}
							if (t == (uint8)NkTimelineTangent::Auto) {
								// Une courbe douce qui passe par la cle.
								float32 qx = 0.f, qy = 0.f;
								for (int32 i = 0; i <= 8; ++i) {
									const float32 u = (float32)i / 8.f;
									const float32 x = cx - L + u * L * 2.f;
									const float32 y = cy + d * 0.7f * std::cos(u * 3.14159f);
									if (i > 0) {
										Trait(c, qx, qy, x, y, role, c.M("stroke_w"));
									}
									qx = x;
									qy = y;
								}
								Losange(c, cx, cy, d * 0.35f, tr);
								break;
							}
							Trait(c, cx - L, cy + ya, cx, cy, tr, c.M("stroke_w"));
							Trait(c, cx, cy, cx + L, cy + yb, tr, c.M("stroke_w"));
							c.p.Fill(NkPaintRect{cx - L - d * 0.2f, cy + ya - d * 0.2f, d * 0.4f, d * 0.4f}, tr);
							c.p.Fill(NkPaintRect{cx + L - d * 0.2f, cy + yb - d * 0.2f, d * 0.4f, d * 0.4f}, tr);
							Losange(c, cx, cy, d * 0.4f, role);
							break;
						}
						// Les interpolations : la courbe elle-meme, tracee par le repli
						// (les douceurs de NKAnima), et la forme de cle qu'elle donne.
						if (b >= NkTimelineButton::InterpFirst && b < NkTimelineButton::Count) {
							const uint8 ip = (uint8)b - (uint8)NkTimelineButton::InterpFirst;
							NkTimelineTrack t;
							t.channels = 1;
							NkTimelineKey k0, k1;
							k0.time = 0.f;
							k0.interp = ip;
							k1.time = 1.f;
							k1.v[0] = 1.f;
							t.keys.PushBack(k0);
							t.keys.PushBack(k1);
							const float32 x0 = rc.x + rc.w * 0.18f, x1 = rc.x + rc.w * 0.82f;
							const float32 y0 = rc.y + rc.h * 0.78f, y1 = rc.y + rc.h * 0.3f;
							float32 qx = 0.f, qy = 0.f;
							for (int32 i = 0; i <= 12; ++i) {
								const float32 u = (float32)i / 12.f;
								float32 v[4];
								m.EvaluateFallback(t, u * 0.999f, v);
								const float32 x = x0 + (x1 - x0) * u, y = y0 + (y1 - y0) * v[0];
								if (i > 0) {
									Trait(c, qx, qy, x, y, role, c.M("stroke_w"));
								}
								qx = x;
								qy = y;
							}
							FormeCle(c, x0, y0, d * 0.4f, ip, c.p.ColorOf(c.s.keySelected));
						}
						break;
					}
				}
			}

			void DessinerBarre(Ctx &c) {
				const NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				c.p.Fill(pl.toolbar, c.s.headerBg);
				c.p.HLine(pl.toolbar.x, pl.toolbar.y + pl.toolbar.h - c.M("stroke_w"), pl.toolbar.w, c.s.border);
				// Feuille | Courbes : un segment a deux moities (la moitie active pleine).
				{
					const NkPaintRect &rc = c.r.buttons[(uint8)NkTimelineButton::Mode];
					if (rc.w > 0.f) {
						const float32 rd = c.M("round");
						if (c.Over(rc)) {
							c.r.hoveredButton = (int32)NkTimelineButton::Mode;
						}
						c.p.Fill(rc, c.s.inputBg, rd);
						const float32 w1 = c.p.TextWidth("Feuille") + c.M("pad") * 2.f;
						const NkPaintRect a{rc.x, rc.y, w1, rc.h}, b{rc.x + w1, rc.y, rc.w - w1, rc.h};
						c.p.Fill(m.curveMode ? b : a, c.s.accent, rd);
						c.p.Text(a, "Feuille", m.curveMode ? c.s.textMuted : c.s.textOnAccent, NkTextAlign::Center);
						c.p.Text(b, "Courbes", m.curveMode ? c.s.textOnAccent : c.s.textMuted, NkTextAlign::Center);
					}
				}
				// Accrocher : un aimant et son mot.
				{
					const NkPaintRect &rc = c.r.buttons[(uint8)NkTimelineButton::Snap];
					if (rc.w > 0.f) {
						FondBouton(c, rc, m.snap, true, (int32)NkTimelineButton::Snap);
						const uint16 role = m.snap ? c.s.textOnAccent : c.s.text;
						const float32 bw = c.M("button_w");
						const float32 cx = rc.x + bw * 0.5f, cy = rc.y + rc.h * 0.5f, d = rc.h * 0.24f;
						const float32 ep = c.M("stroke_w") * 2.f;
						Trait(c, cx - d, cy - d, cx - d, cy + d * 0.3f, role, ep);
						Trait(c, cx + d, cy - d, cx + d, cy + d * 0.3f, role, ep);
						Trait(c, cx - d, cy + d * 0.3f, cx, cy + d, role, ep);
						Trait(c, cx, cy + d, cx + d, cy + d * 0.3f, role, ep);
						c.p.Text(NkPaintRect{rc.x + bw - c.M("pad") * 0.5f, rc.y, rc.w - bw, rc.h}, "Accrocher", role,
								 NkTextAlign::Left);
					}
				}
				Bouton(c, NkTimelineButton::FrameAll, "Cadrer", false, true);
				const uint32 nb = m.SelectionCount();
				const uint32 nbTout = nb + m.ClipSelectionCount();
				auto icone = [&](NkTimelineButton b, bool enfonce, bool actif) {
					const NkPaintRect &rc = c.r.buttons[(uint8)b];
					if (rc.w <= 0.f) {
						return;
					}
					FondBouton(c, rc, enfonce, actif, (int32)b);
					Icone(c, b, enfonce ? c.s.textOnAccent : (actif ? c.s.text : c.s.textMuted));
				};
				icone(NkTimelineButton::AddKey, false, !m.tracks.Empty());
				icone(NkTimelineButton::Delete, false, nbTout > 0 || m.hostKeys);
				icone(NkTimelineButton::Copy, false, nb > 0);
				icone(NkTimelineButton::Paste, false, !m.clipboard.Empty() || m.hostKeys);
				icone(NkTimelineButton::AddMarker, false, true);
				icone(NkTimelineButton::Undo, false, m.hostKeys ? m.hostCanUndo : m.CanUndo());
				icone(NkTimelineButton::Redo, false, m.hostKeys ? m.hostCanRedo : m.CanRedo());
				const uint8 tang = m.SelectionTangent();
				for (uint8 i = 0; i < (uint8)NkTimelineTangent::Count; ++i) {
					icone((NkTimelineButton)((uint8)NkTimelineButton::TangentFirst + i), tang == i, nb > 0);
				}
				const uint8 commune = m.SelectionInterp();
				for (uint8 i = 0; i < (uint8)NkTimelineInterp::Count; ++i) {
					icone((NkTimelineButton)((uint8)NkTimelineButton::InterpFirst + i), commune == i, nb > 0);
				}
				// A droite : les images par seconde.
				char txt[32];
				std::snprintf(txt, sizeof(txt), "%.0f im/s", (double)m.fps);
				const float32 w = c.p.TextWidth(txt) + c.M("pad") * 2.f;
				c.p.Text(NkPaintRect{pl.toolbar.x + pl.toolbar.w - w, pl.toolbar.y, w - c.M("pad"), pl.toolbar.h}, txt,
						 c.s.textMuted, NkTextAlign::Right);
			}

			/// La barre de lecture du bas : transport, compteur, defilement.
			void DessinerTransport(Ctx &c) {
				const NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				c.p.Fill(pl.transport, c.s.headerBg);
				c.p.HLine(pl.transport.x, pl.transport.y, pl.transport.w, c.s.border);
				const NkTimelineButton tr[] = {NkTimelineButton::First,		NkTimelineButton::PrevKey, NkTimelineButton::PrevFrame,
											   NkTimelineButton::Play,		NkTimelineButton::NextFrame, NkTimelineButton::NextKey,
											   NkTimelineButton::Last,		NkTimelineButton::Loop};
				for (NkTimelineButton b : tr) {
					const NkPaintRect &rc = c.r.buttons[(uint8)b];
					if (rc.w <= 0.f) {
						continue;
					}
					const bool enfonce = (b == NkTimelineButton::Play && m.playing) || (b == NkTimelineButton::Loop && m.loop);
					FondBouton(c, rc, enfonce, true, (int32)b);
					Icone(c, b, enfonce ? c.s.textOnAccent : c.s.text);
				}
				char txt[48];
				std::snprintf(txt, sizeof(txt), "%gx", (double)m.speed);
				Bouton(c, NkTimelineButton::Speed, txt, m.speed != 1.f, true);
				// Le compteur : l'image et le temps, comme le champ d'UE5.
				const NkPaintRect &v = c.r.buttons[(uint8)NkTimelineButton::Speed];
				const float32 pad = c.M("pad");
				const float32 x0 = (v.w > 0.f ? v.x + v.w : pl.transportLeft.x) + pad;
				const NkPaintRect cpt{x0, pl.transport.y, pl.transportLeft.x + pl.transportLeft.w - x0 - pad, pl.transport.h};
				std::snprintf(txt, sizeof(txt), "%04d / %d   %.2f s", m.FrameOf(m.cursor), m.FrameOf(m.duration), (double)m.cursor);
				if (cpt.w > 0.f) {
					c.p.Text(cpt, txt, c.s.text, NkTextAlign::Right);
				}
				// La barre de defilement : l'etendue, la plage de lecture, le pouce.
				const NkPaintRect &sc = pl.scroll;
				if (sc.w <= 0.f) {
					return;
				}
				float32 lo, hi;
				NkPaintRect pouce;
				Defilement(c, lo, hi, pouce);
				const float32 rd = c.M("round");
				c.p.Fill(sc, c.s.panelBg, rd);
				const float32 k = sc.w / (hi - lo > 1e-6f ? hi - lo : 1e-6f);
				const float32 xa = sc.x + (m.PlayStart() - lo) * k, xb = sc.x + (m.PlayEnd() - lo) * k;
				c.p.FillColor(NkPaintRect{xa, sc.y + sc.h * 0.35f, xb - xa, sc.h * 0.3f}, Voile(c.p, c.s.textMuted, 0x50u), rd);
				const bool tenu = m.gesture >= (uint8)NkTimelineGesture::ScrollThumb && m.gesture <= (uint8)NkTimelineGesture::ScrollEdgeR;
				c.p.FillColor(pouce, Voile(c.p, c.Over(pouce) || tenu ? c.s.accent : c.s.textMuted, 0x90u), rd);
				const float32 e = c.M("stroke_w") * 2.f;
				c.p.Fill(NkPaintRect{pouce.x, pouce.y, e, pouce.h}, c.s.text);
				c.p.Fill(NkPaintRect{pouce.x + pouce.w - e, pouce.y, e, pouce.h}, c.s.text);
				const float32 xc = sc.x + (m.cursor - lo) * k;
				c.p.VLine(xc, sc.y, sc.h, c.s.playhead);
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
				const NkPaintRect &rg = pl.ruler;
				c.p.Fill(rg, c.s.headerBg);
				c.p.Fill(pl.listHead, c.s.headerBg);
				c.p.HLine(rg.x, rg.y + rg.h - c.M("stroke_w"), rg.w, c.s.border);
				c.p.HLine(pl.listHead.x, pl.listHead.y + pl.listHead.h - c.M("stroke_w"), pl.listHead.w, c.s.border);
				// « + Piste » : un bouton plein, comme le « + Track » d'UE5.
				{
					const NkPaintRect &rc = c.r.buttons[(uint8)NkTimelineButton::AddTrack];
					const bool survol = c.Over(rc);
					if (survol) {
						c.r.hoveredButton = (int32)NkTimelineButton::AddTrack;
					}
					c.p.Fill(rc, survol ? c.s.accent : c.s.buttonBg, c.M("round"));
					c.p.Text(rc, "+ Piste", survol ? c.s.textOnAccent : c.s.text, NkTextAlign::Center);
					// A cote : le nombre de pistes et de cles (le resume d'UE5).
					uint32 nc = 0;
					for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
						nc += (uint32)m.tracks[i].keys.Size() + (uint32)m.tracks[i].clips.Size();
					}
					char txt[48];
					std::snprintf(txt, sizeof(txt), "%u pistes  %u cles", (unsigned)m.tracks.Size(), (unsigned)nc);
					const float32 x = rc.x + rc.w + c.M("pad");
					c.p.Text(NkPaintRect{x, pl.listHead.y, pl.listHead.x + pl.listHead.w - x - c.M("pad"), pl.listHead.h}, txt,
							 c.s.textMuted, NkTextAlign::Right);
				}
				c.p.PushClip(rg);
				const float32 mi = rg.y + rg.h * 0.5f;
				// Hors de la plage de lecture : un voile (les deux etages).
				const float32 xs = NkTimelineTimeToX(m, pl.area, m.PlayStart());
				const float32 xe = NkTimelineTimeToX(m, pl.area, m.PlayEnd());
				if (xs > rg.x) {
					c.p.FillColor(NkPaintRect{rg.x, rg.y, xs - rg.x, rg.h}, Voile(c.p, c.s.border, 0x80u));
				}
				if (xe < rg.x + rg.w) {
					c.p.FillColor(NkPaintRect{xe, rg.y, rg.x + rg.w - xe, rg.h}, Voile(c.p, c.s.border, 0x80u));
				}
				// L'etage BAS : les images (graduations mineures et nombres).
				const int32 pas = PasGraduation(c);
				const float32 f = m.FrameDuration();
				const int32 i0 = (int32)std::floor(m.viewStart / f);
				const int32 i1 = (int32)std::ceil(m.viewEnd / f);
				const int32 mineur = pas >= 5 ? pas / 5 : 1;
				char txt[24];
				for (int32 i = i0 - (i0 % mineur) - mineur; i <= i1; i += mineur) {
					if (i < 0) {
						continue;
					}
					const float32 x = NkTimelineTimeToX(m, pl.area, (float32)i * f);
					if (i % pas == 0) {
						c.p.VLine(x, mi, rg.h * 0.5f, c.s.textMuted);
						std::snprintf(txt, sizeof(txt), "%d", i);
						c.p.Text(NkPaintRect{x + c.M("button_gap") * 2.f, mi, c.M("label_min_px"), rg.h * 0.5f - c.M("stroke_w")},
								 txt, c.s.text, NkTextAlign::Left);
					} else {
						c.p.VLine(x, rg.y + rg.h * 0.8f, rg.h * 0.2f, c.s.border);
					}
				}
				// L'etage HAUT : les SECONDES (un pas rond qui laisse de la place).
				{
					const float32 span = m.viewEnd - m.viewStart;
					const float32 pxS = pl.area.w / (span > 1e-6f ? span : 1e-6f);
					static const float32 kPasS[] = {0.05f, 0.1f, 0.25f, 0.5f, 1.f, 2.f, 5.f, 10.f, 30.f, 60.f, 300.f};
					float32 ps = 600.f;
					for (float32 p : kPasS) {
						if (p * pxS >= c.M("label_min_px") * 1.4f) {
							ps = p;
							break;
						}
					}
					for (float32 t = std::floor(m.viewStart / ps) * ps; t <= m.viewEnd; t += ps) {
						if (t < -1e-4f) {
							continue;
						}
						const float32 x = NkTimelineTimeToX(m, pl.area, t);
						c.p.VLine(x, rg.y + rg.h * 0.3f, rg.h * 0.2f, c.s.border);
						if (ps < 1.f) {
							std::snprintf(txt, sizeof(txt), "%.2f s", (double)(t < 0.f ? 0.f : t));
						} else {
							std::snprintf(txt, sizeof(txt), "%.0f s", (double)t);
						}
						c.p.Text(NkPaintRect{x + c.M("button_gap") * 2.f, rg.y, c.M("label_min_px") * 1.4f, rg.h * 0.5f}, txt,
								 c.s.textMuted, NkTextAlign::Left);
					}
				}
				// Les poignees de PLAGE : un crochet vert au debut, rouge a la fin.
				{
					const float32 rw = c.M("range_w");
					const uint16 vi = c.Role(c.s.rangeIn, c.s.accent), vo = c.Role(c.s.rangeOut, c.s.playhead);
					c.p.Fill(NkPaintRect{xs - c.M("stroke_w"), rg.y, c.M("stroke_w") * 2.f, rg.h}, vi);
					Triangle(c, xs, rg.y, xs + rw, rg.y, xs, rg.y + rw * 1.2f, vi);
					c.p.Fill(NkPaintRect{xe - c.M("stroke_w"), rg.y, c.M("stroke_w") * 2.f, rg.h}, vo);
					Triangle(c, xe, rg.y, xe - rw, rg.y, xe, rg.y + rw * 1.2f, vo);
				}
				// Les MARQUEURS : un drapeau et son nom dans l'etage haut.
				{
					const uint16 mr = c.Role(c.s.marker, c.s.keySelected);
					const float32 mh = c.M("marker_h");
					for (uint32 k = 0; k < (uint32)m.markers.Size(); ++k) {
						const NkTimelineMarker &mk = m.markers[k];
						const float32 x = NkTimelineTimeToX(m, pl.area, mk.time);
						const float32 y = rg.y + c.M("button_gap");
						const float32 xy[10] = {x, y, x + mh, y, x + mh, y + mh * 0.7f, x + mh * 0.5f, y + mh, x, y + mh};
						Poly(c, xy, 5, c.p.ColorOf(mk.selected ? c.s.accent : mr), x + mh * 0.5f, y + mh * 0.5f, mh * 0.5f);
						c.p.VLine(x, y, rg.h - c.M("button_gap"), mr);
						const float32 tw = c.p.TextWidth(mk.name.CStr());
						const NkPaintRect nr{x + mh + c.M("button_gap"), rg.y, tw + c.M("pad"), rg.h * 0.5f};
						c.p.FillColor(nr, Voile(c.p, c.s.headerBg, 0xD0u));
						c.p.Text(nr, mk.name.CStr(), mk.selected ? c.s.accent : mr, NkTextAlign::Left);
						const float32 cx = c.in.mouseX, cy = c.in.mouseY;
						if (cx >= x - mh && cx <= x + mh * 2.f && cy >= rg.y && cy <= rg.y + rg.h * 0.5f) {
							c.r.hoveredMarker = mk.id;
						}
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
						c.p.FillColor(NkPaintRect{pl.area.x, l.y, pl.area.w, l.h}, Voile(c.p, c.s.headerBg, 0xC0u));
					} else if (k % 2 == 1) {
						c.p.FillColor(NkPaintRect{pl.area.x, l.y, pl.area.w, l.h}, Voile(c.p, c.s.rowAltBg, 0x70u));
					}
					c.p.HLine(pl.area.x, l.y + l.h - c.M("stroke_w"), pl.area.w, c.s.border);
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
				// Hors de la plage de lecture et au-dela de la duree : un voile.
				const float32 x0 = NkTimelineTimeToX(m, pl.area, m.PlayStart());
				const float32 xd = NkTimelineTimeToX(m, pl.area, m.PlayEnd());
				const float32 xf = NkTimelineTimeToX(m, pl.area, m.duration);
				if (x0 > pl.area.x) {
					c.p.FillColor(NkPaintRect{pl.area.x, pl.area.y, x0 - pl.area.x, pl.area.h}, Voile(c.p, c.s.border, 0x60u));
				}
				if (xd < pl.area.x + pl.area.w) {
					c.p.FillColor(NkPaintRect{xd, pl.area.y, pl.area.x + pl.area.w - xd, pl.area.h}, Voile(c.p, c.s.border, 0x60u));
				}
				if (xf < pl.area.x + pl.area.w) {
					c.p.FillColor(NkPaintRect{xf, pl.area.y, pl.area.x + pl.area.w - xf, pl.area.h}, Voile(c.p, c.s.border, 0x40u));
					c.p.VLine(xf, pl.area.y, pl.area.h, c.s.textMuted);
				}
				c.p.PopClip();
			}

			/// Un petit bouton de piste a lettre (M, S) ou le verrou.
			void BoutonPiste(Ctx &c, const NkPaintRect &rc, uint8 genre, bool actif, bool ligneSurvol) {
				if (rc.w <= 0.f) {
					return;
				}
				const bool survol = c.Over(rc);
				const uint16 plein = genre == 0 ? c.s.playhead : (genre == 1 ? c.s.keySelected : c.s.accent);
				const float32 rd = c.M("round");
				if (actif) {
					c.p.Fill(rc, plein, rd);
				} else if (survol) {
					c.p.Fill(rc, c.s.inputBg, rd);
				}
				const uint16 role = actif ? c.s.textOnAccent : (ligneSurvol || survol ? c.s.textMuted : c.s.border);
				if (genre == 2) {
					// Le cadenas : l'anse et le corps.
					const float32 cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.55f, d = rc.h * 0.2f;
					const float32 ep = c.M("stroke_w") * 1.5f;
					c.p.Fill(NkPaintRect{cx - d, cy - d * 0.2f, d * 2.f, d * 1.5f}, role);
					c.p.Line(cx - d * 0.6f, cy - d * 0.2f, cx - d * 0.6f, cy - d * 1.1f, role, ep);
					c.p.Line(cx + d * 0.6f, cy - d * 0.2f, cx + d * 0.6f, cy - d * 1.1f, role, ep);
					c.p.Line(cx - d * 0.6f, cy - d * 1.1f, cx + d * 0.6f, cy - d * 1.1f, role, ep);
					return;
				}
				c.p.Text(rc, genre == 0 ? "M" : "S", role, NkTextAlign::Center);
			}

			/// L'icone d'une piste, selon son genre.
			void IconePiste(Ctx &c, const NkPaintRect &rc, const NkTimelineTrack &tr, const float32 v[4], bool terne) {
				const float32 cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f, d = rc.h * 0.2f;
				const uint32 a = terne ? 0x70u : 0xFFu;
				switch (tr.kind) {
					case NkTimelineValueKind::Couleur: {
						auto octet = [](float32 x) {
							x = x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
							return (uint32)(x * 255.f + 0.5f);
						};
						c.p.FillColor(NkPaintRect{cx - d * 1.3f, cy - d * 1.3f, d * 2.6f, d * 2.6f},
									  (octet(v[0]) << 24) | (octet(v[1]) << 16) | (octet(v[2]) << 8) | a, c.M("round"));
						break;
					}
					case NkTimelineValueKind::Palier: {
						const uint16 r = c.s.textMuted;
						const float32 ep = c.M("stroke_w") * 1.5f;
						c.p.Line(cx - d * 1.3f, cy + d, cx - d * 0.2f, cy + d, r, ep);
						c.p.Line(cx - d * 0.2f, cy + d, cx - d * 0.2f, cy - d, r, ep);
						c.p.Line(cx - d * 0.2f, cy - d, cx + d * 1.3f, cy - d, r, ep);
						break;
					}
					case NkTimelineValueKind::Clips: {
						// Une pellicule.
						const uint16 r = c.Role(c.s.clip, c.s.accent);
						c.p.FillColor(NkPaintRect{cx - d * 1.4f, cy - d, d * 2.8f, d * 2.f}, Voile(c.p, r, a));
						for (int32 i = 0; i < 3; ++i) {
							const float32 x = cx - d + (float32)i * d;
							c.p.FillColor(NkPaintRect{x - d * 0.2f, cy - d * 0.8f, d * 0.4f, d * 0.35f}, Voile(c.p, c.s.panelBg, a));
							c.p.FillColor(NkPaintRect{x - d * 0.2f, cy + d * 0.45f, d * 0.4f, d * 0.35f}, Voile(c.p, c.s.panelBg, a));
						}
						break;
					}
					default: {
						// Des nombres : une barre par canal, a sa couleur.
						const uint32 n = tr.channels;
						const float32 hb = d * 2.4f / (float32)(n > 0 ? n : 1);
						for (uint32 ch = 0; ch < n; ++ch) {
							c.p.FillColor(NkPaintRect{cx - d * 1.2f, cy - d * 1.2f + hb * (float32)ch, d * 2.4f, hb * 0.7f},
										  Voile(c.p, c.s.channel[ch], a));
						}
						break;
					}
				}
			}

			void DessinerColonne(Ctx &c) {
				NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				const float32 pad = c.M("pad");
				const float32 g = c.M("button_gap");
				c.p.PushClip(pl.list);
				for (uint32 k = 0; k < (uint32)c.r.rows.Size(); ++k) {
					const NkTimelineRow &l = c.r.rows[k];
					const NkPaintRect ligne{pl.list.x, l.y, pl.list.w, l.h};
					const bool survolLigne = c.Over(ligne);
					NkTimelineTrack *tr = m.Track(l.track);
					const ZonesLigne z = Zones(c, l, tr);
					if (l.track == 0) {
						c.p.Fill(ligne, c.s.headerBg);
						c.p.HLine(ligne.x, l.y + l.h - c.M("stroke_w"), ligne.w, c.s.border);
						// Le chevron, TRACE : ▸ replie, ▾ ouvert.
						const float32 cx = z.chevron.x + z.chevron.w * 0.5f, cy = l.y + l.h * 0.5f;
						const float32 d = l.h * 0.16f;
						if (m.IsCollapsed(l.object)) {
							Triangle(c, cx - d * 0.6f, cy - d, cx - d * 0.6f, cy + d, cx + d, cy, c.s.textMuted);
						} else {
							Triangle(c, cx - d, cy - d * 0.6f, cx + d, cy - d * 0.6f, cx, cy + d, c.s.textMuted);
						}
						// L'icone d'objet : pleine pour l'objet anime, creuse pour ses enfants.
						const float32 ix = z.icone.x + z.icone.w * 0.5f, s2 = l.h * 0.2f;
						if (l.object.Empty()) {
							c.p.Fill(NkPaintRect{ix - s2, cy - s2, s2 * 2.f, s2 * 2.f}, c.s.accent, c.M("round"));
						} else {
							c.p.Outline(NkPaintRect{ix - s2, cy - s2, s2 * 2.f, s2 * 2.f}, c.s.accent, c.s.headerBg, c.M("round"));
						}
						c.p.Text(z.libelle, NomObjet(m, l.object), c.s.text, NkTextAlign::Left);
						for (uint8 f = 0; f < 3; ++f) {
							const NkPaintRect &rc = f == 0 ? z.muet : (f == 1 ? z.solo : z.verrou);
							BoutonPiste(c, rc, f, m.ObjectFlag(l.object, f), survolLigne);
						}
						continue;
					}
					if (tr == nullptr) {
						continue;
					}
					const bool active = m.activeTrack == tr->id;
					const bool terne = !m.TrackActive(*tr);
					if (active) {
						c.p.FillColor(ligne, Voile(c.p, c.s.accent, 0x55u));
						c.p.Fill(NkPaintRect{ligne.x, ligne.y, g, ligne.h}, c.s.accent);
					} else if (k % 2 == 1) {
						c.p.FillColor(ligne, Voile(c.p, c.s.rowAltBg, 0x70u));
					}
					c.p.HLine(ligne.x, l.y + l.h - c.M("stroke_w"), ligne.w, c.s.border);
					if (survolLigne) {
						c.r.hoveredTrack = tr->id;
					}
					float32 v[4];
					c.ValueAtCursor(*tr, v);
					IconePiste(c, z.icone, *tr, v, terne);
					c.p.Text(z.libelle, tr->label.Empty() ? tr->property.CStr() : tr->label.CStr(),
							 terne ? c.s.textMuted : c.s.text, NkTextAlign::Left);
					if (EstClips(*tr)) {
						// Une piste de clips : son mode, son masque, son influence.
						char txt[48];
						std::snprintf(txt, sizeof(txt), "%s%s%s", tr->additive ? "Additif" : "Remplace",
									  tr->mask.Empty() ? "" : " · ", tr->mask.Empty() ? "" : tr->mask.CStr());
						const NkPaintRect &cz = z.canal[0];
						if (cz.w > 0.f) {
							const float32 tw = c.p.TextWidth(txt);
							c.p.Text(NkPaintRect{cz.x - tw - pad, l.y, tw + pad, l.h}, txt, c.s.textMuted, NkTextAlign::Right);
							c.p.Fill(cz, c.s.inputBg, c.M("round"));
							c.p.FillColor(NkPaintRect{cz.x, cz.y + cz.h - g, cz.w * (tr->weight < 0.f ? 0.f : (tr->weight > 1.f ? 1.f : tr->weight)), g},
										  c.p.ColorOf(c.s.accent));
							std::snprintf(txt, sizeof(txt), "%.2f", (double)tr->weight);
							c.p.Text(cz, txt, c.s.text, NkTextAlign::Center);
						}
					} else if (NkTimelineParam(c.s, "show_values") > 0.5f) {
						// Les valeurs au curseur, une case par canal, liseree de sa couleur.
						for (uint32 ch = 0; ch < tr->channels; ++ch) {
							const NkPaintRect &cz = z.canal[ch];
							if (cz.w <= 0.f) {
								continue;
							}
							c.p.Fill(cz, c.s.inputBg, c.M("round"));
							const bool visible = !m.curveMode || tr->showChannel[ch];
							c.p.Fill(z.bascule[ch], visible ? c.s.channel[ch] : c.s.border);
							char txt[24];
							if (tr->kind == NkTimelineValueKind::Palier) {
								std::snprintf(txt, sizeof(txt), "%.0f", (double)v[ch]);
							} else {
								std::snprintf(txt, sizeof(txt), "%.2f", (double)v[ch]);
							}
							c.p.Text(NkPaintRect{cz.x + g * 3.f, cz.y, cz.w - g * 4.f, cz.h}, txt, terne ? c.s.textMuted : c.s.text,
									 NkTextAlign::Right);
						}
					}
					// ◁ ◆ ▷ : les cles de CETTE piste (UE5). Une piste de clips : « + ».
					const float32 kr = c.M("key_r");
					const float32 cyc = l.y + l.h * 0.5f;
					if (EstClips(*tr)) {
						const float32 cx = z.cle.x + z.cle.w * 0.5f, q = kr * 0.9f;
						const uint16 rr = c.Over(z.cle) ? c.s.accent : c.s.textMuted;
						c.p.Line(cx - q, cyc, cx + q, cyc, rr, c.M("stroke_w") * 2.f);
						c.p.Line(cx, cyc - q, cx, cyc + q, rr, c.M("stroke_w") * 2.f);
					} else {
						const bool cleIci = m.KeyAt(*tr, m.cursor) >= 0;
						const float32 cx = z.cle.x + z.cle.w * 0.5f;
						Losange(c, cx, cyc, kr, cleIci ? c.s.keySelected : (c.Over(z.cle) ? c.s.text : c.s.textMuted));
						if (!cleIci) {
							Losange(c, cx, cyc, kr * 0.5f, active ? c.s.headerBg : c.s.panelBg);
						}
						const uint16 nav = survolLigne ? c.s.textMuted : c.s.border;
						const float32 px = z.prec.x + z.prec.w * 0.5f, sx = z.suiv.x + z.suiv.w * 0.5f, d = kr * 0.8f;
						Triangle(c, px + d * 0.5f, cyc - d, px + d * 0.5f, cyc + d, px - d * 0.7f, cyc, c.Over(z.prec) ? c.s.text : nav);
						Triangle(c, sx - d * 0.5f, cyc - d, sx - d * 0.5f, cyc + d, sx + d * 0.7f, cyc, c.Over(z.suiv) ? c.s.text : nav);
					}
					// « × » au survol de la ligne : retirer la piste.
					if (survolLigne) {
						const float32 cx = z.retirer.x + z.retirer.w * 0.5f, cy = z.retirer.y + z.retirer.h * 0.5f;
						const float32 q = kr * 0.7f;
						const uint16 rr = c.Over(z.retirer) ? c.s.playhead : c.s.textMuted;
						c.p.Line(cx - q, cy - q, cx + q, cy + q, rr, c.M("stroke_w") * 1.5f);
						c.p.Line(cx + q, cy - q, cx - q, cy + q, rr, c.M("stroke_w") * 1.5f);
					}
					BoutonPiste(c, z.muet, 0, tr->muted, survolLigne);
					BoutonPiste(c, z.solo, 1, tr->solo, survolLigne);
					BoutonPiste(c, z.verrou, 2, tr->locked, survolLigne);
				}
				c.p.PopClip();
				c.p.VLine(pl.list.x + pl.list.w, pl.list.y, pl.list.h, c.s.border);
			}

			/// Une SECTION (le fond teinte d'UE5 sous les cles d'une piste).
			void Section(Ctx &c, float32 t0, float32 t1, float32 y, float32 h, uint16 role, uint32 alpha) {
				const float32 kr = c.M("key_r");
				const float32 x0 = NkTimelineTimeToX(c.m, c.plan.area, t0) - kr * 1.6f;
				const float32 x1 = NkTimelineTimeToX(c.m, c.plan.area, t1) + kr * 1.6f;
				c.p.FillColor(NkPaintRect{x0, y, x1 - x0, h}, Voile(c.p, role, alpha), c.M("round"));
			}

			/// Les clips d'une piste de clips (NLA) : barres arrondies, nom, fondus,
			/// cycles de boucle, chevauchements.
			void DessinerClips(Ctx &c, const NkTimelineRow &l, const NkTimelineTrack &tr) {
				const NkTimelineModel &m = c.m;
				const NkPaintRect &a = c.plan.area;
				const float32 g = c.M("button_gap");
				const float32 y0 = l.y + g * 2.f, y1 = l.y + l.h - g * 2.f;
				const bool terne = !m.TrackActive(tr);
				const uint16 corps = tr.additive ? c.Role(c.s.clipAlt, c.s.keySelected) : c.Role(c.s.clip, c.s.accent);
				for (uint32 q = 0; q < (uint32)tr.clips.Size(); ++q) {
					const NkTimelineClip &cl = tr.clips[q];
					const float32 x0 = NkTimelineTimeToX(m, a, cl.start), x1 = NkTimelineTimeToX(m, a, cl.End());
					if (x1 < a.x || x0 > a.x + a.w) {
						continue;
					}
					const NkPaintRect b{x0, y0, x1 - x0, y1 - y0};
					const bool survol = c.Over(b);
					if (survol) {
						c.r.hoveredClip = cl.id;
						c.r.hoveredTrack = tr.id;
					}
					c.p.FillColor(b, Voile(c.p, corps, terne ? 0x50u : (cl.selected ? 0xF0u : 0xB0u)), c.M("round"));
					// Les cycles de la boucle : un trait a chaque tour du clip.
					if (cl.loop && cl.sourceLength > 1e-3f && cl.rate > 1e-3f) {
						const float32 tour = cl.sourceLength / cl.rate;
						float32 t = cl.start + (cl.sourceLength - std::fmod(cl.offset, cl.sourceLength)) / cl.rate;
						for (int32 n = 0; t < cl.End() - 1e-4f && n < 256; ++n, t += tour) {
							const float32 x = NkTimelineTimeToX(m, a, t);
							c.p.VLine(x, y0 + (y1 - y0) * 0.55f, (y1 - y0) * 0.45f, c.s.panelBg);
						}
					}
					// Les FONDUS : le coin au-dessus de la rampe s'assombrit.
					const uint32 ombre = Voile(c.p, c.s.panelBg, 0xA0u);
					if (cl.blendIn > 1e-4f) {
						const float32 xi = NkTimelineTimeToX(m, a, cl.start + cl.blendIn);
						const float32 xy[6] = {x0, y0, xi, y0, x0, y1};
						Poly(c, xy, 3, ombre, x0, y0, g);
						c.p.Line(x0, y1, xi, y0, c.s.text, c.M("stroke_w"));
					}
					if (cl.blendOut > 1e-4f) {
						const float32 xo = NkTimelineTimeToX(m, a, cl.End() - cl.blendOut);
						const float32 xy[6] = {xo, y0, x1, y0, x1, y1};
						Poly(c, xy, 3, ombre, x1, y0, g);
						c.p.Line(xo, y0, x1, y1, c.s.text, c.M("stroke_w"));
					}
					// Les poignees de fondu (au survol ou choisi).
					if (survol || cl.selected) {
						const float32 hs = c.M("edge_px") * 0.7f;
						const float32 xi = NkTimelineTimeToX(m, a, cl.start + cl.blendIn);
						const float32 xo = NkTimelineTimeToX(m, a, cl.End() - cl.blendOut);
						c.p.Fill(NkPaintRect{xi - hs * 0.5f, y0 - hs * 0.5f, hs, hs}, c.s.text);
						c.p.Fill(NkPaintRect{xo - hs * 0.5f, y0 - hs * 0.5f, hs, hs}, c.s.text);
						// Les bords qui rognent.
						c.p.Fill(NkPaintRect{x0, y0, g, y1 - y0}, c.s.text);
						c.p.Fill(NkPaintRect{x1 - g, y0, g, y1 - y0}, c.s.text);
					}
					if (cl.selected) {
						c.p.HLine(x0, y0, x1 - x0, c.s.keySelected);
						c.p.HLine(x0, y1 - c.M("stroke_w"), x1 - x0, c.s.keySelected);
					}
					// Le nom (et le poids s'il n'est pas 1).
					char txt[80];
					if (cl.weight < 0.999f) {
						std::snprintf(txt, sizeof(txt), "%s  x%.2f", cl.name.CStr(), (double)cl.weight);
					} else {
						std::snprintf(txt, sizeof(txt), "%s", cl.name.CStr());
					}
					const float32 xt = Max(x0, a.x) + c.M("pad");
					c.p.Text(NkPaintRect{xt, y0, x1 - xt - c.M("pad"), (y1 - y0) * 0.6f}, txt,
							 cl.selected ? c.s.textOnAccent : c.s.text, NkTextAlign::Left);
				}
				// Les CHEVAUCHEMENTS : la zone ou deux clips se fondent, hachuree.
				for (uint32 q = 0; q + 1 < (uint32)tr.clips.Size(); ++q) {
					for (uint32 r = q + 1; r < (uint32)tr.clips.Size(); ++r) {
						const float32 t0 = Max(tr.clips[q].start, tr.clips[r].start);
						const float32 t1 = Min(tr.clips[q].End(), tr.clips[r].End());
						if (t1 <= t0) {
							continue;
						}
						const float32 x0 = NkTimelineTimeToX(m, a, t0), x1 = NkTimelineTimeToX(m, a, t1);
						c.p.Line(x0, y1, x1, y0, c.s.text, c.M("stroke_w"));
						c.p.Line(x0, y0, x1, y1, c.s.text, c.M("stroke_w"));
					}
				}
			}

			void DessinerFeuille(Ctx &c) {
				const NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				const float32 kr = c.M("key_r");
				const bool resume = NkTimelineParam(c.s, "show_summary") > 0.5f;
				c.p.PushClip(pl.area);
				for (uint32 k = 0; k < (uint32)c.r.rows.Size(); ++k) {
					const NkTimelineRow &l = c.r.rows[k];
					const float32 cy = l.y + l.h * 0.5f;
					const float32 sh = l.h * 0.62f;
					if (l.track == 0) {
						if (!resume) {
							continue;
						}
						// Le RESUME d'un objet (et de ses descendants) : sa section, et
						// un petit losange par image ou l'un d'eux a une cle.
						float32 t0 = 1e30f, t1 = -1e30f;
						for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
							const NkTimelineTrack &tr = m.tracks[i];
							if (NkTimelineModel::IsUnderObject(tr.object, l.object) && !tr.keys.Empty()) {
								t0 = Min(t0, tr.keys[0].time);
								t1 = Max(t1, tr.keys[tr.keys.Size() - 1].time);
							}
						}
						if (t0 <= t1) {
							Section(c, t0, t1, cy - sh * 0.5f, sh, c.s.textMuted, 0x30u);
						}
						for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
							const NkTimelineTrack &tr = m.tracks[i];
							if (!NkTimelineModel::IsUnderObject(tr.object, l.object)) {
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
					if (EstClips(*tr)) {
						DessinerClips(c, l, *tr);
						continue;
					}
					const bool terne = !m.TrackActive(*tr);
					if (!tr->keys.Empty()) {
						// La SECTION de la piste, teintee comme dans UE5.
						const uint16 teinte = tr->kind == NkTimelineValueKind::Palier ? c.s.textMuted : c.s.accent;
						Section(c, tr->keys[0].time, tr->keys[tr->keys.Size() - 1].time, cy - sh * 0.4f, sh * 0.8f, teinte,
								terne ? 0x14u : 0x2Au);
					}
					for (uint32 q = 0; q < (uint32)tr->keys.Size(); ++q) {
						const NkTimelineKey &key = tr->keys[q];
						const float32 x = NkTimelineTimeToX(m, pl.area, key.time);
						// Une valeur qui TIENT (palier, ou deux cles egales) : une barre
						// jusqu'a la suivante, comme les « holds » de Blender.
						if (q + 1 < (uint32)tr->keys.Size()) {
							const NkTimelineKey &n = tr->keys[q + 1];
							bool egales = true;
							for (uint32 ch = 0; ch < tr->channels; ++ch) {
								egales = egales && Abs(n.v[ch] - key.v[ch]) < 1e-5f;
							}
							if (key.interp == (uint8)NkTimelineInterp::Palier || egales) {
								const float32 x2 = NkTimelineTimeToX(m, pl.area, n.time);
								c.p.FillColor(NkPaintRect{x, cy - kr * 0.35f, x2 - x, kr * 0.7f},
											  Voile(c.p, key.selected && n.selected ? c.s.keySelected : c.s.textMuted, 0x60u));
							}
						}
					}
					for (uint32 q = 0; q < (uint32)tr->keys.Size(); ++q) {
						const NkTimelineKey &key = tr->keys[q];
						const float32 x = NkTimelineTimeToX(m, pl.area, key.time);
						const bool survol = Abs(x - c.in.mouseX) <= c.M("hit_r") && Abs(cy - c.in.mouseY) <= c.M("hit_r");
						if (survol) {
							c.r.hoveredTrack = tr->id;
							c.r.hoveredKey = (int32)q;
						}
						Cle(c, x, cy, kr, key.interp, key.selected, survol, terne || tr->locked);
					}
				}
				c.p.PopClip();
			}

			void AjusterValeurs(Ctx &c) {
				NkTimelineModel &m = c.m;
				if (!m.curveAutoFit || m.gesture == (uint8)NkTimelineGesture::MoveKeys ||
					m.gesture == (uint8)NkTimelineGesture::MoveTangent) {
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
					std::snprintf(txt, sizeof(txt), "%.2f", (double)(Abs(v) < pas * 0.01f ? 0.f : v)); // jamais « -0.00 »
					c.p.Text(NkPaintRect{pl.area.x + c.M("pad"), y - c.M("row_h"), c.M("value_w") * 2.f, c.M("row_h")}, txt,
							 c.s.textMuted, NkTextAlign::Left);
				}
				const float32 ech = c.M("curve_samples_px") > 1.f ? c.M("curve_samples_px") : 1.f;
				const float32 kr = c.M("key_r");
				const uint16 tg = c.Role(c.s.tangent, c.s.keySelected);
				for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
					const NkTimelineTrack &tr = m.tracks[i];
					if (!CourbeMontree(m, tr) || tr.keys.Empty()) {
						continue;
					}
					const bool terne = !m.TrackActive(tr);
					const float32 tPremiere = tr.keys[0].time, tDerniere = tr.keys[tr.keys.Size() - 1].time;
					for (uint32 ch = 0; ch < tr.channels; ++ch) {
						if (!tr.showChannel[ch]) {
							continue;
						}
						// La courbe ECHANTILLONNEE par l'interpolation de l'hote : celle
						// que le jeu jouera. Hors des cles (pre / post-infini d'UE5) : en
						// tirets, plus pale.
						float32 px = pl.area.x, py = 0.f;
						int32 n = 0;
						for (float32 x = pl.area.x; x <= pl.area.x + pl.area.w + ech; x += ech, ++n) {
							const float32 t = NkTimelineXToTime(m, pl.area, x);
							float32 v[4];
							c.Eval(tr, t, v);
							const float32 y = NkTimelineValueToY(m, pl.area, v[ch]);
							if (x > pl.area.x) {
								const bool dehors = t < tPremiere || t > tDerniere;
								if (!dehors) {
									c.p.Line(px, py, x, y, terne ? c.s.textMuted : c.s.channel[ch], c.M("curve_w"));
								} else if (n % 2 == 0) {
									c.p.Line(px, py, x, y, c.s.textMuted, c.M("stroke_w"));
								}
							}
							px = x;
							py = y;
						}
						for (uint32 k = 0; k < (uint32)tr.keys.Size(); ++k) {
							const NkTimelineKey &key = tr.keys[k];
							const float32 x = NkTimelineTimeToX(m, pl.area, key.time);
							const float32 y = NkTimelineValueToY(m, pl.area, key.v[ch]);
							// Les POIGNEES DE TANGENTE des cles choisies, du cote d'une courbe.
							if (key.selected && !tr.locked) {
								float32 pin, pout;
								m.KeySlopes(tr, k, ch, pin, pout);
								for (int32 cote = 0; cote < 2; ++cote) {
									const bool so = cote == 1;
									const bool utile = so ? (k + 1 < (uint32)tr.keys.Size() && key.interp == (uint8)NkTimelineInterp::Courbe)
														  : (k > 0 && tr.keys[k - 1].interp == (uint8)NkTimelineInterp::Courbe);
									if (!utile) {
										continue;
									}
									float32 hx, hy;
									BoutTangente(c, key.time, key.v[ch], so ? pout : pin, so, hx, hy);
									c.p.Line(x, y, hx, hy, tg, c.M("stroke_w"));
									const float32 hs = kr * 0.7f;
									c.p.Fill(NkPaintRect{hx - hs, hy - hs, hs * 2.f, hs * 2.f}, tg);
								}
							}
							Cle(c, x, y, kr, key.interp, key.selected, false, terne);
						}
					}
				}
				c.p.PopClip();
			}

			void DessinerMarqueursZone(Ctx &c) {
				const NkTimelineModel &m = c.m;
				const Plan &pl = c.plan;
				const uint16 mr = c.Role(c.s.marker, c.s.keySelected);
				c.p.PushClip(pl.area);
				for (uint32 k = 0; k < (uint32)m.markers.Size(); ++k) {
					const float32 x = NkTimelineTimeToX(m, pl.area, m.markers[k].time);
					Tirets(c, x, pl.area.y, pl.area.y + pl.area.h, m.markers[k].selected ? c.s.accent : mr);
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
				const float32 mi = pl.ruler.y + pl.ruler.h * 0.5f;
				c.p.Fill(NkPaintRect{x - ep * 0.5f, mi, ep, pl.ruler.y + pl.ruler.h - mi + pl.area.h}, c.s.playhead);
				// La TETE : une etiquette a pointe, le numero d'image dedans (UE5).
				char txt[16];
				std::snprintf(txt, sizeof(txt), "%d", m.FrameOf(m.cursor));
				const float32 w = c.p.TextWidth(txt) + c.M("pad") * 2.f;
				const float32 h = pl.ruler.h * 0.5f - c.M("button_gap");
				const float32 y0 = mi;
				const float32 xy[10] = {x - w * 0.5f, y0, x + w * 0.5f, y0, x + w * 0.5f, y0 + h * 0.75f, x, y0 + h,
										x - w * 0.5f, y0 + h * 0.75f};
				Poly(c, xy, 5, c.p.ColorOf(c.s.playhead), x, y0 + h * 0.5f, h * 0.5f);
				c.p.Text(NkPaintRect{x - w * 0.5f, y0, w, h * 0.75f}, txt, c.s.textOnAccent, NkTextAlign::Center);
			}

			void DessinerBoite(Ctx &c) {
				const NkTimelineModel &m = c.m;
				c.p.PushClip(c.plan.area);
				// La boite de TRANSFORMATION des cles choisies, et ses deux bords.
				NkPaintRect b;
				if (m.gesture != (uint8)NkTimelineGesture::Box && Boite(c, b)) {
					const uint16 r = c.s.keySelected;
					c.p.HLine(b.x, b.y, b.w, r);
					c.p.HLine(b.x, b.y + b.h, b.w, r);
					const float32 e = c.M("edge_px") * 0.5f;
					c.p.Fill(NkPaintRect{b.x - e * 0.5f, b.y, e, b.h}, r);
					c.p.Fill(NkPaintRect{b.x + b.w - e * 0.5f, b.y, e, b.h}, r);
				}
				if (m.gesture == (uint8)NkTimelineGesture::Box && m.gestureMoved) {
					const float32 x0 = Min(m.gestureX0, c.in.mouseX);
					const float32 y0 = Min(m.gestureY0, c.in.mouseY);
					const NkPaintRect s{x0, y0, Abs(c.in.mouseX - m.gestureX0), Abs(c.in.mouseY - m.gestureY0)};
					c.p.FillColor(s, Voile(c.p, c.s.accent, 0x30u));
					c.p.HLine(s.x, s.y, s.w, c.s.accent);
					c.p.HLine(s.x, s.y + s.h, s.w, c.s.accent);
					c.p.VLine(s.x, s.y, s.h, c.s.accent);
					c.p.VLine(s.x + s.w, s.y, s.h, c.s.accent);
				}
				c.p.PopClip();
			}

			/// L'infobulle du bouton survole (les barres sont faites d'icones).
			void DessinerBulle(Ctx &c, const NkPaintRect &rect) {
				if (c.r.hoveredButton < 0 || c.m.gesture != (uint8)NkTimelineGesture::None) {
					return;
				}
				const char *t = Bulle((uint8)c.r.hoveredButton);
				if (t == nullptr) {
					return;
				}
				const NkPaintRect &b = c.r.buttons[c.r.hoveredButton];
				const float32 pad = c.M("pad");
				const float32 w = c.p.TextWidth(t) + pad * 2.f, h = c.M("row_h");
				float32 x = b.x;
				x = x + w > rect.x + rect.w ? rect.x + rect.w - w : x;
				// Sous la barre du haut, au-dessus de celle du bas.
				const bool bas = b.y > c.plan.transport.y - 1.f;
				const float32 y = bas ? b.y - h - c.M("button_gap") : b.y + b.h + c.M("button_gap");
				const NkPaintRect r{x, y, w, h};
				c.p.Fill(r, c.s.headerBg, c.M("round"));
				c.p.Outline(r, c.s.border, c.s.headerBg, c.M("round"));
				c.p.Text(r, t, c.s.text, NkTextAlign::Center);
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
			// Le suivi du curseur pendant la lecture (le zoom adouci : apres la molette).
			if (m.playing && m.gesture == (uint8)NkTimelineGesture::None) {
				m.FollowCursor();
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
			m.GlideStep(c.Raw("glide"));
			// Les lignes ont pu changer (un objet replie, une piste retiree).
			r.rows.Clear();
			Lignes(c);
			for (uint32 k = 0; k < (uint32)r.rows.Size(); ++k) {
				NkTimelineRow &l = r.rows[k];
				const NkTimelineTrack *tr = m.Track(l.track);
				const ZonesLigne z = Zones(c, l, tr);
				l.muteButton = z.muet;
				l.soloButton = z.solo;
				l.lockButton = z.verrou;
				if (tr != nullptr) {
					l.keyButton = z.cle;
					l.prevKeyButton = z.prec;
					l.nextKeyButton = z.suiv;
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
				DessinerFeuille(c);
			}
			DessinerMarqueursZone(c);
			DessinerBoite(c);
			DessinerCurseur(c);
			DessinerBarre(c);
			DessinerTransport(c);
			DessinerBulle(c, rect);
			return r;
		}

	} // namespace editorkit
} // namespace nkentseu
