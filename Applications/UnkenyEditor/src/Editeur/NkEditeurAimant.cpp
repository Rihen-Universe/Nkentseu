// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurAimant.cpp
// DESCRIPTION: L'AIMANT de la vue (Rihen, 01/10) : en deplacant un bloc
//              (sprite, forme, collisionneur) ou une poignee (sommet d'un
//              collisionneur, point d'un polygone), coller aux SOMMETS, aux
//              ARETES et aux FACES des autres objets -- l'accrochage aux sommets
//              d'Unreal (V maintenue).
//
// LA REGLE
//   - Le rayon de capture est en PIXELS d'ecran (ui.aimantRayonPx) : il reste
//     le meme sous la main quel que soit le zoom.
//   - PRIORITE sommet > arete > face : un sommet dans le rayon l'emporte sur une
//     arete plus proche ; une arete sur la face.
//   - La FACE n'a pas de rayon : un point ENTRE dans un autre objet (fermé) est
//     ramene au point le plus proche de sa surface -- on « pose » une caisse
//     sur un sol en la poussant dedans.
//   - Un BLOC cherche par tous ses points d'accroche (coins et centre) ; la
//     correction est celle du meilleur.
//   - Allume par le bouton de la barre flottante (a cote de la grille), ou le
//     temps que V est maintenue. L'accrochage a la grille s'applique d'abord,
//     l'aimant ensuite : un accroche l'emporte sur la grille.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurPlacer.h"

#include <cmath>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkVec2;

		namespace {
			constexpr uint32 CONTOUR_MAX = NK_FORME_CONTOUR_MAX;
			constexpr uint32 COINS_MAX = 64u;

			/// La geometrie d'une entite, en MONDE : son contour (aretes) et ses
			/// coins (sommets). Ce que l'oeil voit d'elle : sa forme 2D, sinon son
			/// sprite, sinon son collisionneur. Rend faux si elle n'a rien.
			struct NkGeometrie {
					NkVec2f contour[CONTOUR_MAX];
					uint32 n = 0;
					bool ferme = true;
					NkVec2f coins[COINS_MAX];
					uint32 nbCoins = 0;
					NkVec2f centre{0.f, 0.f};
			};

			bool Geometrie(NkScene &s, ecs::NkEntityId id, NkGeometrie &g) {
				ecs::NkWorld &w = s.Monde();
				const NkTransform2D *t = w.Get<NkTransform2D>(id);
				if (t == nullptr || !s.EstActive(id) || w.Has<NkCorpsMou2D>(id)) {
					return false;
				}
				g.n = 0;
				g.nbCoins = 0;
				g.ferme = true;
				g.centre = t->position;
				if (const NkRenduForme2D *f = w.Get<NkRenduForme2D>(id)) {
					if (f->visible) {
						g.n = NkContourForme2D(*f, g.contour, CONTOUR_MAX, g.ferme);
						for (uint32 i = 0; i < g.n; ++i) {
							g.contour[i] = t->VersMonde(g.contour[i]);
						}
						// Les COINS : ceux d'un polygone (pas les points d'une courbe).
						const bool courbe = f->genre == NkGenreForme2D::NK_CERCLE || f->genre == NkGenreForme2D::NK_ELLIPSE ||
											f->genre == NkGenreForme2D::NK_CAPSULE;
						if (!courbe) {
							bool fc = true;
							g.nbCoins = NkSommetsForme2D(*f, g.coins, COINS_MAX, fc);
							for (uint32 i = 0; i < g.nbCoins; ++i) {
								g.coins[i] = t->VersMonde(g.coins[i]);
							}
						}
						return g.n >= 2u;
					}
				}
				if (const NkSprite2D *sp = w.Get<NkSprite2D>(id)) {
					if (sp->visible) {
						const float32 W = sp->taille.x, H = sp->taille.y;
						const float32 ox = -sp->pivot.x * W, oy = -sp->pivot.y * H;
						g.contour[0] = t->VersMonde(NkVec2f(ox, oy));
						g.contour[1] = t->VersMonde(NkVec2f(ox + W, oy));
						g.contour[2] = t->VersMonde(NkVec2f(ox + W, oy + H));
						g.contour[3] = t->VersMonde(NkVec2f(ox, oy + H));
						g.n = 4u;
						for (uint32 i = 0; i < 4u; ++i) {
							g.coins[i] = g.contour[i];
						}
						g.nbCoins = 4u;
						return true;
					}
				}
				if (const NkCollisionneur2D *c = w.Get<NkCollisionneur2D>(id)) {
					g.n = NkContourCollisionneur2D(*t, *c, g.contour, CONTOUR_MAX, g.ferme);
					if (c->forme == NkForme2D::NK_BOITE || c->forme == NkForme2D::NK_POLYGONE || c->forme == NkForme2D::NK_CHAINE) {
						for (uint32 i = 0; i < g.n && i < COINS_MAX; ++i) {
							g.coins[i] = g.contour[i];
						}
						g.nbCoins = g.n < COINS_MAX ? g.n : COINS_MAX;
					}
					return g.n >= 2u;
				}
				return false;
			}

			/// Le point du segment [a, b] le plus proche de p.
			NkVec2f Projete(const NkVec2f &p, const NkVec2f &a, const NkVec2f &b) {
				const float32 abx = b.x - a.x, aby = b.y - a.y;
				const float32 l2 = abx * abx + aby * aby;
				float32 u = l2 > 1.0e-12f ? ((p.x - a.x) * abx + (p.y - a.y) * aby) / l2 : 0.f;
				u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
				return NkVec2f(a.x + abx * u, a.y + aby * u);
			}

			float32 Dist2(const NkVec2f &a, const NkVec2f &b) {
				return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
			}

			/// Dedans un contour FERME (pair-impair) ?
			bool Dedans(const NkGeometrie &g, const NkVec2f &p) {
				if (!g.ferme || g.n < 3u) {
					return false;
				}
				bool d = false;
				for (uint32 i = 0, j = g.n - 1u; i < g.n; j = i++) {
					const NkVec2f &a = g.contour[i];
					const NkVec2f &b = g.contour[j];
					if (((a.y > p.y) != (b.y > p.y)) && (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)) {
						d = !d;
					}
				}
				return d;
			}

			/// Le point de la surface (contour) le plus proche de p.
			NkVec2f Surface(const NkGeometrie &g, const NkVec2f &p) {
				NkVec2f meilleur = g.contour[0];
				float32 d = 1.0e30f;
				const uint32 nbSeg = g.ferme ? g.n : g.n - 1u;
				for (uint32 i = 0; i < nbSeg; ++i) {
					const NkVec2f q = Projete(p, g.contour[i], g.contour[(i + 1u) % g.n]);
					const float32 di = Dist2(p, q);
					if (di < d) {
						d = di;
						meilleur = q;
					}
				}
				return meilleur;
			}

			/// Le meilleur accroche de l'ensemble de points `pts` (ceux qu'on
			/// deplace), contre toutes les entites sauf `exclue` (et ses enfants).
			bool Chercher(NkEditeurModele &m, const NkVec2f *pts, uint32 nb, float32 rayon, ecs::NkEntityId exclue,
						  NkAccrocheAimant &sortie) {
				NkScene &s = m.scene;
				NkVector<ecs::NkEntityId> ids;
				s.Entites(ids);
				const float32 r2 = rayon * rayon;
				float32 meilleurSommet = r2, meilleureArete = r2, meilleureFace = -1.f;
				NkAccrocheAimant sommet, arete, face;
				NkGeometrie *g = memory::NkGetDefaultAllocator().New<NkGeometrie>(); // gros : sur le tas
				for (uint32 k = 0; k < ids.Size(); ++k) {
					const ecs::NkEntityId id = ids[k];
					if (id == exclue || (exclue.IsValid() && s.Parent(id) == exclue) || NkEditeurCacheDansLaVue(m, id) ||
						!Geometrie(s, id, *g)) {
						continue;
					}
					for (uint32 i = 0; i < nb; ++i) {
						const NkVec2f &p = pts[i];
						// Les SOMMETS de l'autre.
						for (uint32 j = 0; j < g->nbCoins; ++j) {
							const float32 d = Dist2(p, g->coins[j]);
							if (d <= meilleurSommet) {
								meilleurSommet = d;
								sommet.trouve = true;
								sommet.genre = NkGenreAimant::NK_SOMMET;
								sommet.point = g->coins[j];
								sommet.delta = NkVec2f(g->coins[j].x - p.x, g->coins[j].y - p.y);
								sommet.cible = id;
							}
						}
						// Ses ARETES.
						const uint32 nbSeg = g->ferme ? g->n : g->n - 1u;
						for (uint32 j = 0; j < nbSeg; ++j) {
							const NkVec2f q = Projete(p, g->contour[j], g->contour[(j + 1u) % g->n]);
							const float32 d = Dist2(p, q);
							if (d <= meilleureArete) {
								meilleureArete = d;
								arete.trouve = true;
								arete.genre = NkGenreAimant::NK_ARETE;
								arete.point = q;
								arete.delta = NkVec2f(q.x - p.x, q.y - p.y);
								arete.cible = id;
							}
						}
						// Sa FACE : le point le plus ENFONCE est ramene a la surface.
						if (Dedans(*g, p)) {
							const NkVec2f q = Surface(*g, p);
							const float32 d = Dist2(p, q);
							if (d > meilleureFace) {
								meilleureFace = d;
								face.trouve = true;
								face.genre = NkGenreAimant::NK_FACE;
								face.point = q;
								face.delta = NkVec2f(q.x - p.x, q.y - p.y);
								face.cible = id;
							}
						}
					}
				}
				memory::NkGetDefaultAllocator().Delete(g);
				sortie = sommet.trouve ? sommet : (arete.trouve ? arete : face);
				return sortie.trouve;
			}
		} // namespace

		// =====================================================================
		bool NkEditeurAimantActif(const NkEditeurCadre &c) {
			const bool v = c.ctx.input.keyDown[static_cast<int32>(nkgui::NkGuiKey::V)];
			return c.ui.aimant || v;
		}

		bool NkEditeurAimanterPoint(NkEditeurModele &m, const NkVec2f &p, float32 rayon, ecs::NkEntityId exclue,
									NkAccrocheAimant &sortie) {
			sortie = NkAccrocheAimant();
			return Chercher(m, &p, 1u, rayon, exclue, sortie);
		}

		bool NkEditeurAimanterBloc(NkEditeurModele &m, ecs::NkEntityId bloc, const NkVec2f &cible, float32 rayon,
								   NkAccrocheAimant &sortie) {
			sortie = NkAccrocheAimant();
			const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(bloc);
			NkGeometrie *g = memory::NkGetDefaultAllocator().New<NkGeometrie>();
			bool ok = false;
			if (t != nullptr && Geometrie(m.scene, bloc, *g)) {
				// Les points d'accroche du bloc A LA CIBLE : ses coins et son centre
				// (une courbe n'a que son centre et les extremes de son contour).
				const NkVec2f d(cible.x - t->position.x, cible.y - t->position.y);
				NkVec2f pts[COINS_MAX + 5u];
				uint32 n = 0;
				pts[n++] = cible;
				for (uint32 i = 0; i < g->nbCoins && n < COINS_MAX + 1u; ++i) {
					pts[n++] = NkVec2f(g->coins[i].x + d.x, g->coins[i].y + d.y);
				}
				if (g->nbCoins == 0u && g->n > 0u) {
					// Les extremes (gauche, droite, bas, haut) d'une courbe.
					NkVec2f e[4] = {g->contour[0], g->contour[0], g->contour[0], g->contour[0]};
					for (uint32 i = 1; i < g->n; ++i) {
						const NkVec2f &q = g->contour[i];
						e[0] = q.x < e[0].x ? q : e[0];
						e[1] = q.x > e[1].x ? q : e[1];
						e[2] = q.y < e[2].y ? q : e[2];
						e[3] = q.y > e[3].y ? q : e[3];
					}
					for (int32 i = 0; i < 4; ++i) {
						pts[n++] = NkVec2f(e[i].x + d.x, e[i].y + d.y);
					}
				}
				ok = Chercher(m, pts, n, rayon, bloc, sortie);
			}
			memory::NkGetDefaultAllocator().Delete(g);
			return ok;
		}

		NkVec2f NkEditeurAimanterDeplacement(NkEditeurCadre &c, const NkVec2f &cible) {
			NkEditeurModele &m = c.m;
			if (!NkEditeurAimantActif(c) || !m.aSelection || m.scene.Monde().Has<NkCorpsMou2D>(m.selection)) {
				return cible;
			}
			NkAccrocheAimant a;
			const float32 rayon = c.ui.aimantRayonPx / m.scene.Camera().Zoom();
			if (!NkEditeurAimanterBloc(m, m.selection, cible, rayon, a)) {
				return cible;
			}
			c.ui.aimantVu = true;
			c.ui.aimantPoint = a.point;
			c.ui.aimantGenre = static_cast<int32>(a.genre);
			return NkVec2f(cible.x + a.delta.x, cible.y + a.delta.y);
		}

		NkVec2f NkEditeurAimanterPoignee(NkEditeurCadre &c, const NkVec2f &p, ecs::NkEntityId exclue) {
			NkEditeurModele &m = c.m;
			if (!NkEditeurAimantActif(c)) {
				return p;
			}
			NkAccrocheAimant a;
			const float32 rayon = c.ui.aimantRayonPx / m.scene.Camera().Zoom();
			if (!NkEditeurAimanterPoint(m, p, rayon, exclue, a)) {
				return p;
			}
			c.ui.aimantVu = true;
			c.ui.aimantPoint = a.point;
			c.ui.aimantGenre = static_cast<int32>(a.genre);
			return a.point;
		}

		void NkEditeurDessinerAimant(NkEditeurCadre &c, nkgui::NkGuiDrawList &dl) {
			NkEditeurInterface &ui = c.ui;
			if (!ui.aimantVu) {
				return;
			}
			ui.aimantVu = false; // pose par la souris de cette trame, peint a la suivante
			const NkVec2f e = c.m.scene.Camera().MondeVersEcran(ui.aimantPoint);
			const NkColor magenta(255, 64, 200, 255);
			const NkColor fond(20, 24, 30, 230);
			switch (static_cast<NkGenreAimant>(ui.aimantGenre)) {
				case NkGenreAimant::NK_SOMMET: {
					// Un losange plein : un SOMMET (le plus fort).
					const NkVec2 l[4] = {NkVec2{e.x, e.y - 8.f}, NkVec2{e.x + 8.f, e.y}, NkVec2{e.x, e.y + 8.f}, NkVec2{e.x - 8.f, e.y}};
					dl.AddTriangleFilled(l[0], l[1], l[2], magenta);
					dl.AddTriangleFilled(l[0], l[2], l[3], magenta);
					dl.AddPolyline(l, 4, fond, 1.5f, true);
					break;
				}
				case NkGenreAimant::NK_ARETE:
					// Un cercle : un point d'une ARETE.
					dl.AddCircleFilled(e, 6.f, fond);
					dl.AddCircle(e, 6.f, magenta, 2.f);
					break;
				default:
					// Un carre et sa croix : la SURFACE d'une face.
					dl.AddRectFilled(nkgui::NkRect{e.x - 6.f, e.y - 6.f, 12.f, 12.f}, fond, 2.f);
					dl.AddRect(nkgui::NkRect{e.x - 6.f, e.y - 6.f, 12.f, 12.f}, magenta, 2.f, 2.f);
					dl.AddLine(NkVec2{e.x - 4.f, e.y}, NkVec2{e.x + 4.f, e.y}, magenta, 1.5f);
					break;
			}
			// Le rayon de capture, discret.
			dl.AddCircle(e, ui.aimantRayonPx, NkColor(255, 64, 200, 90), 1.f);
		}

	} // namespace editeur
} // namespace nkentseu
