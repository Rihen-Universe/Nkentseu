// -----------------------------------------------------------------------------
// FICHIER: NKEditorKit/Components/NkCanevasNoeuds.cpp
// DESCRIPTION: Le canevas nodal generique (NkCanevasNoeuds.h) : dessin d'un
//              NkNodeGraph et gestes d'edition.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKEditorKit/Components/NkCanevasNoeuds.h"

#include <cstdio>
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
			/// L'indice de la prise `k` PARMI celles de son sens (sa rangee).
			int32 Rangee(const graph::NkNode &n, int32 k) {
				const graph::NkSocketDir sens = n.sockets[static_cast<uint32>(k)].dir;
				int32 r = 0;
				for (int32 i = 0; i < k; ++i) {
					r += n.sockets[static_cast<uint32>(i)].dir == sens ? 1 : 0;
				}
				return r;
			}
			int32 NbRangees(const graph::NkNode &n) {
				int32 ein = 0, eout = 0;
				for (uint32 i = 0; i < n.sockets.Size(); ++i) {
					(n.sockets[i].dir == graph::NkSocketDir::Input ? ein : eout)++;
				}
				return ein > eout ? ein : eout;
			}
			void Texte(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 x, float32 top, const char *s, const NkColor &c,
					   float32 zoom, float32 maxW = -1.f) {
				if (f == nullptr || f->Face() == nullptr || s == nullptr) {
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
				return f != nullptr && s != nullptr ? f->MeasureWidth(s) * zoom : 0.f;
			}
			NkColor CouleurPrise(const graph::NkNodeGraph &g, const graph::NkSocket &s, const NkStyleCanevas &st,
								 const NkDomaineCanevas &d) {
				if (s.family == graph::NkSocketFamily::Exec) {
					return st.exec;
				}
				return d.CouleurType != nullptr ? d.CouleurType(d.donnees, g, s.type) : st.attenue;
			}
			/// Une cubique de `a` (sortie) a `b` (entree), echantillonnee.
			void Fil(nkgui::NkGuiDrawList &dl, const NkVec2 &a, const NkVec2 &b, const NkColor &c, float32 epaisseur) {
				const float32 dx = (b.x - a.x) * 0.5f;
				const float32 tire = dx < 40.f ? 40.f : dx;
				const NkVec2 c1{a.x + tire, a.y};
				const NkVec2 c2{b.x - tire, b.y};
				NkVec2 pts[25];
				for (int32 i = 0; i <= 24; ++i) {
					const float32 t = static_cast<float32>(i) / 24.f;
					const float32 u = 1.f - t;
					const float32 w0 = u * u * u, w1 = 3.f * u * u * t, w2 = 3.f * u * t * t, w3 = t * t * t;
					pts[i] = NkVec2{w0 * a.x + w1 * c1.x + w2 * c2.x + w3 * b.x, w0 * a.y + w1 * c1.y + w2 * c2.y + w3 * b.y};
				}
				dl.AddPolyline(pts, 25, c, epaisseur);
			}
			void PriseExec(nkgui::NkGuiDrawList &dl, const NkVec2 &p, float32 r, const NkColor &c, bool plein) {
				const NkVec2 a{p.x - r, p.y - r * 1.1f}, b{p.x + r * 1.1f, p.y}, cc{p.x - r, p.y + r * 1.1f};
				if (plein) {
					dl.AddTriangleFilled(a, b, cc, c);
				} else {
					dl.AddLine(a, b, c, 1.4f);
					dl.AddLine(b, cc, c, 1.4f);
					dl.AddLine(cc, a, c, 1.4f);
				}
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
			/// Le champ de valeur d'une entree libre (rectangle ecran), ou vide.
			NkRect Champ(const NkEtatCanevas &e, const NkRect &zone, const graph::NkNode &n, int32 k, const NkStyleCanevas &s,
						 nkgui::NkGuiFont *f) {
				NkVec2 p;
				if (!NkCanevasPrise(e, zone, n, k, s, p)) {
					return NkRect{0.f, 0.f, 0.f, 0.f};
				}
				const float32 z = e.zoom;
				const float32 lx = p.x + (s.rayonPrise + 6.f) * z + Largeur(f, n.sockets[static_cast<uint32>(k)].name.CStr(), z) + 6.f * z;
				const float32 h = (s.hauteurRangee - 6.f) * z;
				const NkRect noeud = NkCanevasRectNoeud(e, zone, n, s);
				float32 w = s.largeurChamp * z;
				const float32 maxi = noeud.x + noeud.w * 0.62f - lx;
				if (w > maxi) {
					w = maxi > 24.f * z ? maxi : 24.f * z;
				}
				return NkRect{lx, p.y - h * 0.5f, w, h};
			}
		} // namespace

		NkVec2 NkCanevasVersEcran(const NkEtatCanevas &e, const NkRect &zone, float32 x, float32 y) noexcept {
			return NkVec2{zone.x + (x - e.vueX) * e.zoom, zone.y + (y - e.vueY) * e.zoom};
		}

		NkVec2 NkCanevasDepuisEcran(const NkEtatCanevas &e, const NkRect &zone, const NkVec2 &p) noexcept {
			return NkVec2{e.vueX + (p.x - zone.x) / e.zoom, e.vueY + (p.y - zone.y) / e.zoom};
		}

		NkRect NkCanevasRectNoeud(const NkEtatCanevas &e, const NkRect &zone, const graph::NkNode &n, const NkStyleCanevas &s) noexcept {
			const NkVec2 p = NkCanevasVersEcran(e, zone, n.x, n.y);
			const float32 h = s.hauteurEntete + static_cast<float32>(NbRangees(n)) * s.hauteurRangee + 6.f;
			return NkRect{p.x, p.y, s.largeurNoeud * e.zoom, h * e.zoom};
		}

		bool NkCanevasPrise(const NkEtatCanevas &e, const NkRect &zone, const graph::NkNode &n, int32 k, const NkStyleCanevas &s,
							NkVec2 &sortie) noexcept {
			if (k < 0 || static_cast<uint32>(k) >= n.sockets.Size()) {
				return false;
			}
			const NkRect r = NkCanevasRectNoeud(e, zone, n, s);
			const bool entree = n.sockets[static_cast<uint32>(k)].dir == graph::NkSocketDir::Input;
			const float32 y = r.y + (s.hauteurEntete + (static_cast<float32>(Rangee(n, k)) + 0.5f) * s.hauteurRangee + 3.f) * e.zoom;
			sortie = NkVec2{entree ? r.x + 10.f * e.zoom : r.x + r.w - 10.f * e.zoom, y};
			return true;
		}

		void NkCanevasCadrer(NkEtatCanevas &e, const NkRect &zone, const graph::NkNodeGraph &g, const NkStyleCanevas &s) noexcept {
			float32 x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
			for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
				const graph::NkNode *n = g.RawNodeAt(i);
				if (n == nullptr || !n->alive) {
					continue;
				}
				const float32 h = s.hauteurEntete + static_cast<float32>(NbRangees(*n)) * s.hauteurRangee + 6.f;
				x0 = n->x < x0 ? n->x : x0;
				y0 = n->y < y0 ? n->y : y0;
				x1 = n->x + s.largeurNoeud > x1 ? n->x + s.largeurNoeud : x1;
				y1 = n->y + h > y1 ? n->y + h : y1;
			}
			if (x1 < x0) {
				e.vueX = -40.f;
				e.vueY = -40.f;
				e.zoom = 1.f;
				return;
			}
			const float32 marge = 40.f;
			float32 z = (zone.w - 2.f * marge) / (x1 - x0);
			const float32 zy = (zone.h - 2.f * marge) / (y1 - y0);
			z = zy < z ? zy : z;
			z = z < 0.4f ? 0.4f : (z > 1.25f ? 1.25f : z);
			e.zoom = z;
			e.vueX = x0 - (zone.w / z - (x1 - x0)) * 0.5f;
			e.vueY = y0 - (zone.h / z - (y1 - y0)) * 0.5f;
		}

		void NkCanevasNoeuds(nkgui::NkGuiDrawList &dl, nkgui::NkGuiInput &in, nkgui::NkGuiFont *police, const NkRect &zone,
							 graph::NkNodeGraph &g, NkEtatCanevas &e, const NkStyleCanevas &s, const NkDomaineCanevas &d) {
			e.modifie = false;
			e.menuDemande = false;
			e.clavierPris = false;
			e.ageRefus += in.dt > 0.f ? in.dt : 1.f / 60.f;
			const NkVec2 souris = in.mousePos;
			const bool dedans = Dans(zone, souris);
			const float32 z = e.zoom;
			const float32 rp = s.rayonPrise * z;

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
					// UTF-8 : le texte d'une entree peut porter des accents.
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
						--l; // les octets de suite d'un caractere UTF-8
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
				// Ces touches sont au canevas : l'editeur ne doit pas les revoir.
				in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Enter)] = false;
				in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Escape)] = false;
				in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Backspace)] = false;
				in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Delete)] = false;
			}

			// ── 2. LES GESTES ───────────────────────────────────────────────────
			// Ce qui est sous le curseur : une prise, un champ, un noeud (le
			// dernier dessine est au-dessus).
			graph::NkNodeId sousNoeud = graph::NK_NODE_INVALID;
			int32 sousPrise = -1;
			graph::NkNodeId sousChampNoeud = graph::NK_NODE_INVALID;
			int32 sousChamp = -1;
			for (uint32 i = g.RawNodeCount(); i-- > 0u;) {
				const graph::NkNode *n = g.RawNodeAt(i);
				if (n == nullptr || !n->alive) {
					continue;
				}
				for (uint32 k = 0; k < n->sockets.Size() && sousPrise < 0; ++k) {
					NkVec2 p;
					if (NkCanevasPrise(e, zone, *n, static_cast<int32>(k), s, p) &&
						Carre(p.x - souris.x) + Carre(p.y - souris.y) <= Carre(rp * 2.2f + 2.f)) {
						sousNoeud = n->id;
						sousPrise = static_cast<int32>(k);
					}
					if (sousChamp < 0 && n->sockets[k].dir == graph::NkSocketDir::Input &&
						n->sockets[k].family == graph::NkSocketFamily::Data && d.TexteDefaut != nullptr &&
						!PriseLiee(g, n->id, static_cast<int32>(k))) {
						const NkRect c = Champ(e, zone, *n, static_cast<int32>(k), s, police);
						if (c.w > 0.f && Dans(c, souris) && !d.TexteDefaut(d.donnees, g, *n, n->sockets[k]).Empty()) {
							sousChampNoeud = n->id;
							sousChamp = static_cast<int32>(k);
						}
					}
				}
				if (sousPrise >= 0) {
					break;
				}
				if (sousNoeud == graph::NK_NODE_INVALID && Dans(NkCanevasRectNoeud(e, zone, *n, s), souris)) {
					sousNoeud = n->id;
					if (sousChamp >= 0) {
						break;
					}
				}
				if (sousNoeud != graph::NK_NODE_INVALID && sousChampNoeud != graph::NK_NODE_INVALID) {
					break;
				}
			}

			if (dedans && e.geste == 0) {
				// La molette : zoom autour du curseur.
				if (in.wheel != 0.f) {
					const NkVec2 avant = NkCanevasDepuisEcran(e, zone, souris);
					float32 nz = e.zoom * (in.wheel > 0.f ? 1.12f : 1.f / 1.12f);
					nz = nz < 0.3f ? 0.3f : (nz > 2.f ? 2.f : nz);
					e.zoom = nz;
					e.vueX = avant.x - (souris.x - zone.x) / nz;
					e.vueY = avant.y - (souris.y - zone.y) / nz;
					in.wheel = 0.f;
				}
				if (in.mouseClicked[0]) {
					if (e.saisieNoeud != graph::NK_NODE_INVALID) {
						validerSaisie();
					}
					if (sousPrise >= 0) {
						e.geste = 2;
						e.gesteNoeud = sousNoeud;
						e.gestePrise = sousPrise;
					} else if (sousChamp >= 0) {
						const graph::NkNode *n = g.Find(sousChampNoeud);
						const NkString t = d.TexteDefaut(d.donnees, g, *n, n->sockets[static_cast<uint32>(sousChamp)]);
						e.saisieNoeud = sousChampNoeud;
						e.saisiePrise = sousChamp;
						std::snprintf(e.saisie, sizeof(e.saisie), "%s", t.CStr());
						e.selection = sousChampNoeud;
					} else if (sousNoeud != graph::NK_NODE_INVALID) {
						const graph::NkNode *n = g.Find(sousNoeud);
						e.selection = sousNoeud;
						e.geste = 1;
						e.gesteNoeud = sousNoeud;
						const NkVec2 p = NkCanevasDepuisEcran(e, zone, souris);
						e.gesteDx = p.x - n->x;
						e.gesteDy = p.y - n->y;
					} else {
						e.selection = graph::NK_NODE_INVALID;
						e.geste = 3;
						e.bouton = 0;
						e.gesteDx = e.vueX;
						e.gesteDy = e.vueY;
						e.appuiX = souris.x;
						e.appuiY = souris.y;
					}
				} else if (in.mouseClicked[1] || in.mouseClicked[2]) {
					const int32 b = in.mouseClicked[1] ? 1 : 2;
					if (b == 1 && sousPrise >= 0) {
						// Couper les fils de cette prise.
						for (uint32 i = 0; i < g.LinkCount(); ++i) {
							const graph::NkLink *l = g.LinkAt(i);
							if (l != nullptr && l->alive &&
								((l->fromNode == sousNoeud && l->fromSocket == sousPrise) ||
								 (l->toNode == sousNoeud && l->toSocket == sousPrise))) {
								g.Disconnect(l->id);
								e.modifie = true;
							}
						}
					} else {
						e.geste = 3;
						e.bouton = b;
						e.gesteDx = e.vueX;
						e.gesteDy = e.vueY;
						e.appuiX = souris.x;
						e.appuiY = souris.y;
					}
				}
			}
			if (e.geste == 1) {
				if (graph::NkNode *n = g.Find(e.gesteNoeud)) {
					const NkVec2 p = NkCanevasDepuisEcran(e, zone, souris);
					const float32 nx = p.x - e.gesteDx, ny = p.y - e.gesteDy;
					if (nx != n->x || ny != n->y) {
						n->x = nx;
						n->y = ny;
					}
				}
				if (!in.mouseDown[0]) {
					e.geste = 0;
					e.modifie = true; // la position est dans le fichier
				}
			} else if (e.geste == 3) {
				e.vueX = e.gesteDx - (souris.x - e.appuiX) / e.zoom;
				e.vueY = e.gesteDy - (souris.y - e.appuiY) / e.zoom;
				if (!in.mouseDown[e.bouton]) {
					// Un clic droit SANS deplacement dans le vide : la palette.
					if (e.bouton == 1 && Carre(souris.x - e.appuiX) + Carre(souris.y - e.appuiY) < 16.f) {
						const NkVec2 p = NkCanevasDepuisEcran(e, zone, souris);
						e.menuDemande = true;
						e.menuX = p.x;
						e.menuY = p.y;
					}
					e.geste = 0;
				}
			} else if (e.geste == 2 && !in.mouseDown[0]) {
				// Le fil est lache : sur une prise, on tente ; le refus se NOMME.
				if (sousPrise >= 0 && !(sousNoeud == e.gesteNoeud && sousPrise == e.gestePrise)) {
					const graph::NkNode *a = g.Find(e.gesteNoeud);
					const graph::NkNode *b = g.Find(sousNoeud);
					if (a != nullptr && b != nullptr) {
						const graph::NkSocket &sa = a->sockets[static_cast<uint32>(e.gestePrise)];
						const graph::NkSocket &sb = b->sockets[static_cast<uint32>(sousPrise)];
						const bool aSort = sa.dir == graph::NkSocketDir::Output;
						const NkString nsa = sa.name, nsb = sb.name;
						const graph::NkNodeId de = aSort ? a->id : b->id;
						const graph::NkNodeId vers = aSort ? b->id : a->id;
						const graph::NkLinkError err =
							g.Connect(de, aSort ? nsa.CStr() : nsb.CStr(), vers, aSort ? nsb.CStr() : nsa.CStr());
						if (err == graph::NkLinkError::Ok) {
							e.modifie = true;
						} else {
							e.refus = NkString::Format("fil refusé : %s", graph::NkLinkErrorName(err));
							e.ageRefus = 0.f;
						}
					}
				}
				e.geste = 0;
			}
			if (dedans && e.saisieNoeud == graph::NK_NODE_INVALID && e.selection != graph::NK_NODE_INVALID &&
				in.KeyPressed(nkgui::NkGuiKey::Delete)) {
				g.RemoveNode(e.selection);
				e.selection = graph::NK_NODE_INVALID;
				e.modifie = true;
				e.clavierPris = true;
				in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Delete)] = false;
			}

			// ── 3. LE DESSIN ────────────────────────────────────────────────────
			dl.PushClipRect(zone);
			dl.AddRectFilled(zone, s.fond);
			{
				const float32 pas = s.pasGrille * e.zoom;
				if (pas >= 6.f) {
					const float32 x0 = zone.x - (e.vueX * e.zoom - static_cast<float32>(static_cast<int32>(e.vueX / s.pasGrille)) * pas);
					int32 kx = static_cast<int32>(e.vueX / s.pasGrille);
					for (float32 x = x0; x < zone.x + zone.w; x += pas, ++kx) {
						dl.AddLine(NkVec2{x, zone.y}, NkVec2{x, zone.y + zone.h}, kx % 4 == 0 ? s.grilleForte : s.grille, 1.f);
					}
					const float32 y0 = zone.y - (e.vueY * e.zoom - static_cast<float32>(static_cast<int32>(e.vueY / s.pasGrille)) * pas);
					int32 ky = static_cast<int32>(e.vueY / s.pasGrille);
					for (float32 y = y0; y < zone.y + zone.h; y += pas, ++ky) {
						dl.AddLine(NkVec2{zone.x, y}, NkVec2{zone.x + zone.w, y}, ky % 4 == 0 ? s.grilleForte : s.grille, 1.f);
					}
				}
			}
			// Les fils, sous les noeuds.
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
				Fil(dl, pa, pb, CouleurPrise(g, sa, s, d), (exec ? 3.f : 2.f) * (e.zoom < 0.6f ? 0.6f : e.zoom));
			}
			// Les noeuds.
			const float32 lh = police != nullptr && police->Face() != nullptr ? police->LineHeight() * z : 14.f * z;
			for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
				const graph::NkNode *n = g.RawNodeAt(i);
				if (n == nullptr || !n->alive) {
					continue;
				}
				const NkRect r = NkCanevasRectNoeud(e, zone, *n, s);
				if (r.x > zone.x + zone.w || r.y > zone.y + zone.h || r.x + r.w < zone.x || r.y + r.h < zone.y) {
					continue;
				}
				const NkColor entete = d.CouleurEntete != nullptr ? d.CouleurEntete(d.donnees, *n) : s.grilleForte;
				dl.AddRectFilled(r, s.corps, 5.f * z);
				dl.AddRectFilled(NkRect{r.x, r.y, r.w, s.hauteurEntete * z}, entete, 5.f * z);
				const bool erreur = n->id == e.enErreur;
				const bool choisi = n->id == e.selection;
				dl.AddRect(r, erreur ? s.erreur : (choisi ? s.selection : s.bord), erreur || choisi ? 2.5f : 1.f, 5.f * z);
				Texte(dl, police, r.x + 8.f * z, r.y + (s.hauteurEntete * z - lh) * 0.5f, n->label.Empty() ? n->type.CStr() : n->label.CStr(),
					  s.texte, z, r.w - 12.f * z);
				for (uint32 k = 0; k < n->sockets.Size(); ++k) {
					const graph::NkSocket &so = n->sockets[k];
					NkVec2 p;
					NkCanevasPrise(e, zone, *n, static_cast<int32>(k), s, p);
					const bool liee = PriseLiee(g, n->id, static_cast<int32>(k));
					const NkColor c = CouleurPrise(g, so, s, d);
					if (so.family == graph::NkSocketFamily::Exec) {
						PriseExec(dl, p, rp, c, liee);
					} else if (liee) {
						dl.AddCircleFilled(p, rp, c);
					} else {
						dl.AddCircle(p, rp, c, 1.5f);
					}
					const bool entree = so.dir == graph::NkSocketDir::Input;
					const char *nom = so.name.CStr();
					if (so.family == graph::NkSocketFamily::Exec && (so.name == "exec" || so.name == "suite")) {
						continue; // les fils d'execution ordinaires n'ont pas besoin d'etiquette
					}
					const float32 tw = Largeur(police, nom, z);
					const float32 tx = entree ? p.x + rp + 6.f * z : p.x - rp - 6.f * z - tw;
					Texte(dl, police, tx, p.y - lh * 0.5f, nom, s.attenue, z);
					// La valeur d'une entree LIBRE, et sa saisie.
					if (entree && so.family == graph::NkSocketFamily::Data && !liee && d.TexteDefaut != nullptr) {
						const bool enSaisie = e.saisieNoeud == n->id && e.saisiePrise == static_cast<int32>(k);
						const NkString v = enSaisie ? NkString(e.saisie) : d.TexteDefaut(d.donnees, g, *n, so);
						if (!enSaisie && v.Empty()) {
							continue;
						}
						const NkRect c2 = Champ(e, zone, *n, static_cast<int32>(k), s, police);
						dl.AddRectFilled(c2, s.champ, 3.f * z);
						dl.AddRect(c2, enSaisie ? s.selection : s.bord, 1.f, 3.f * z);
						Texte(dl, police, c2.x + 4.f * z, c2.y + (c2.h - lh) * 0.5f, v.CStr(), s.texte, z, c2.w - 8.f * z);
						if (enSaisie) {
							const float32 cx = c2.x + 4.f * z + Largeur(police, v.CStr(), z) + 1.f;
							dl.AddLine(NkVec2{cx, c2.y + 3.f}, NkVec2{cx, c2.y + c2.h - 3.f}, s.texte, 1.f);
						}
					}
				}
			}
			// Le fil que l'on tire.
			if (e.geste == 2) {
				if (const graph::NkNode *a = g.Find(e.gesteNoeud)) {
					NkVec2 p;
					if (NkCanevasPrise(e, zone, *a, e.gestePrise, s, p)) {
						const graph::NkSocket &sa = a->sockets[static_cast<uint32>(e.gestePrise)];
						const NkColor c = CouleurPrise(g, sa, s, d);
						if (sa.dir == graph::NkSocketDir::Output) {
							Fil(dl, p, souris, c, 2.f);
						} else {
							Fil(dl, souris, p, c, 2.f);
						}
					}
				}
			}
			// Le dernier refus, NOMME, quelques secondes.
			if (e.ageRefus < 4.f && !e.refus.Empty()) {
				Texte(dl, police, zone.x + 10.f, zone.y + zone.h - lh / (z > 0.f ? z : 1.f) - 10.f, e.refus.CStr(), s.erreur, 1.f);
			}
			dl.PopClipRect();
		}

	} // namespace editorkit
} // namespace nkentseu
