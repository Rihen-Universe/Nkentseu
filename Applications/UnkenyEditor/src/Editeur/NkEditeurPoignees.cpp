// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurPoignees.cpp
// DESCRIPTION: Les POIGNEES du collisionneur de la selection, dans la vue
//              (Rihen, 01/10 : « on doit savoir comment PLACER et DEFINIR les
//              collisions »). Allumees par Details > Collision > « Editer dans
//              la vue » (ui.editionCollision), en EDITION seulement.
//
// LES POIGNEES (repere du collisionneur : decalage puis rotation propre)
//   centre        le DECALAGE (carre plein)
//   rotation      la ROTATION propre (rond, au-dessus de la forme)
//   boite         les quatre coins : demi-taille (symetrique, le centre reste)
//   cercle        le rayon
//   capsule       les deux bouts (demi-longueur) et le flanc (rayon)
//   polygone,     chaque SOMMET se tire ; Ctrl+clic le RETIRE ; le « + » au
//   chaine       milieu d'un cote en INSERE un (et le tire aussitot)
// Chaque geste est annulable (Ctrl+Z) ; un corps rigide est refait a chaque
// pas du geste (ActualiserCorps), et la forme ne refait plus ce collisionneur.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurPlacer.h"

#include <cmath>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			constexpr int32 P_CENTRE = 0;
			constexpr int32 P_ROTATION = 1;
			constexpr int32 P_COIN = 10;	///< + 0..3
			constexpr int32 P_RAYON = 20;
			constexpr int32 P_BOUT = 30;	///< + 0..1
			constexpr int32 P_FLANC = 32;
			constexpr int32 P_SOMMET = 100; ///< + i
			constexpr int32 P_MILIEU = 200; ///< + i : inserer apres le sommet i
			constexpr float32 RAYON_PRISE = 8.f; ///< px

			struct NkPoignee {
					int32 id;
					NkVec2f local; ///< repere du collisionneur
			};

			/// Monde -> repere du collisionneur (l'inverse de NkPointCollision2D).
			NkVec2f VersLocal(const NkTransform2D &t, const NkCollisionneur2D &c, const NkVec2f &w) {
				const float32 ce = std::cos(-t.rotation), se = std::sin(-t.rotation);
				const float32 gx = w.x - t.position.x, gy = w.y - t.position.y;
				const float32 ex = gx * ce - gy * se - c.decalage.x;
				const float32 ey = gx * se + gy * ce - c.decalage.y;
				const float32 cc = std::cos(-c.rotation), sc = std::sin(-c.rotation);
				return NkVec2f(ex * cc - ey * sc, ex * sc + ey * cc);
			}

			/// Monde -> repere de l'ENTITE (pour le decalage et la rotation propre).
			NkVec2f VersEntite(const NkTransform2D &t, const NkVec2f &w) {
				const float32 ce = std::cos(-t.rotation), se = std::sin(-t.rotation);
				const float32 gx = w.x - t.position.x, gy = w.y - t.position.y;
				return NkVec2f(gx * ce - gy * se, gx * se + gy * ce);
			}

			/// La demi-hauteur de la forme (pour poser la poignee de rotation au-dessus).
			float32 Etendue(const NkCollisionneur2D &c) {
				switch (c.forme) {
					case NkForme2D::NK_CERCLE: return c.rayon;
					case NkForme2D::NK_CAPSULE: return c.rayon;
					case NkForme2D::NK_BOITE: return c.demiTaille.y;
					default: {
						float32 h = 0.1f;
						for (uint32 i = 0; i < NkNbSommetsCollision2D(c); ++i) {
							h = math::NkMax(h, c.sommets[i].y);
						}
						return h;
					}
				}
			}

			/// Les poignees du collisionneur `c`, et leur nombre. La poignee de
			/// rotation est a 28 px au-dessus de la forme (`pixel` m par px).
			uint32 Poignees(const NkCollisionneur2D &c, float32 pixel, NkPoignee *p, uint32 cap) {
				uint32 n = 0;
				auto A = [&](int32 id, const NkVec2f &l) {
					if (n < cap) {
						p[n++] = NkPoignee{id, l};
					}
				};
				A(P_CENTRE, NkVec2f(0.f, 0.f));
				if (c.forme != NkForme2D::NK_CERCLE) {
					A(P_ROTATION, NkVec2f(0.f, Etendue(c) + 28.f * pixel));
				}
				switch (c.forme) {
					case NkForme2D::NK_BOITE:
						A(P_COIN + 0, NkVec2f(-c.demiTaille.x, -c.demiTaille.y));
						A(P_COIN + 1, NkVec2f(c.demiTaille.x, -c.demiTaille.y));
						A(P_COIN + 2, NkVec2f(c.demiTaille.x, c.demiTaille.y));
						A(P_COIN + 3, NkVec2f(-c.demiTaille.x, c.demiTaille.y));
						break;
					case NkForme2D::NK_CERCLE:
						A(P_RAYON, NkVec2f(c.rayon, 0.f));
						break;
					case NkForme2D::NK_CAPSULE:
						A(P_BOUT + 0, NkVec2f(-c.demiTaille.x - c.rayon, 0.f));
						A(P_BOUT + 1, NkVec2f(c.demiTaille.x + c.rayon, 0.f));
						A(P_FLANC, NkVec2f(0.f, c.rayon));
						break;
					default: {
						const uint32 ns = NkNbSommetsCollision2D(c);
						const bool ferme = c.forme == NkForme2D::NK_POLYGONE || c.boucle;
						for (uint32 i = 0; i < ns; ++i) {
							A(P_SOMMET + static_cast<int32>(i), c.sommets[i]);
						}
						const uint32 cap2 = c.forme == NkForme2D::NK_POLYGONE ? NK_POLYGONE_CONVEXE_MAX : NK_COLLISION_SOMMETS_MAX;
						if (ns < cap2) {
							const uint32 nbCotes = ferme ? ns : (ns > 0u ? ns - 1u : 0u);
							for (uint32 i = 0; i < nbCotes; ++i) {
								const NkVec2f &a = c.sommets[i];
								const NkVec2f &b = c.sommets[(i + 1u) % ns];
								A(P_MILIEU + static_cast<int32>(i), NkVec2f((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f));
							}
						}
						break;
					}
				}
				return n;
			}

			bool Actives(NkEditeurCadre &c, NkTransform2D *&t, NkCollisionneur2D *&col) {
				NkEditeurModele &m = c.m;
				if (!c.ui.editionCollision || m.etat != NkEtatJeu::NK_EDITION || !m.aSelection || !m.scene.Monde().IsAlive(m.selection)) {
					return false;
				}
				t = m.scene.Monde().Get<NkTransform2D>(m.selection);
				col = m.scene.Monde().Get<NkCollisionneur2D>(m.selection);
				return t != nullptr && col != nullptr;
			}
		} // namespace

		// =====================================================================
		void NkEditeurDessinerPoigneesCollision(NkEditeurCadre &c, nkgui::NkGuiDrawList &dl) {
			NkTransform2D *t = nullptr;
			NkCollisionneur2D *col = nullptr;
			if (!Actives(c, t, col)) {
				return;
			}
			const NkVue2D &cam = c.m.scene.Camera();
			const NkColor vert(0, 224, 122, 255);
			// Le contour, plus epais que la surcouche.
			NkVec2f pts[NK_COLLISION_SOMMETS_MAX + 72u];
			bool ferme = true;
			const uint32 n = NkContourCollisionneur2D(*t, *col, pts, NK_COLLISION_SOMMETS_MAX + 72u, ferme);
			for (uint32 i = 0; i < n; ++i) {
				pts[i] = cam.MondeVersEcran(pts[i]);
			}
			if (n >= 2u) {
				dl.AddPolyline(pts, static_cast<int32>(n), vert, 2.5f, ferme);
			}
			NkPoignee p[96];
			const uint32 np = Poignees(*col, 1.f / cam.Zoom(), p, 96u);
			const NkVec2f centre = cam.MondeVersEcran(NkPointCollision2D(*t, *col, NkVec2f(0.f, 0.f)));
			for (uint32 i = 0; i < np; ++i) {
				const NkVec2f e = cam.MondeVersEcran(NkPointCollision2D(*t, *col, p[i].local));
				const bool vive = c.ui.poigneeTenue == p[i].id || c.ui.poigneeSurvol == p[i].id;
				const float32 r = vive ? 6.5f : 5.f;
				if (p[i].id == P_ROTATION) {
					dl.AddLine(centre, e, NkColor(0, 224, 122, 140), 1.f);
					dl.AddCircleFilled(e, r, vive ? c.pal.selection : vert);
					dl.AddCircle(e, r, NkColor(20, 24, 30, 255), 1.2f);
				} else if (p[i].id >= P_MILIEU) {
					// Le « + » d'un milieu de cote.
					dl.AddCircleFilled(e, r - 1.f, NkColor(20, 24, 30, 200));
					dl.AddLine(NkVec2{e.x - 3.f, e.y}, NkVec2{e.x + 3.f, e.y}, vive ? c.pal.selection : vert, 1.5f);
					dl.AddLine(NkVec2{e.x, e.y - 3.f}, NkVec2{e.x, e.y + 3.f}, vive ? c.pal.selection : vert, 1.5f);
				} else {
					const NkRect q{e.x - r, e.y - r, 2.f * r, 2.f * r};
					dl.AddRectFilled(q, p[i].id == P_CENTRE ? vert : NkColor(245, 245, 245, 255), 1.5f);
					dl.AddRect(q, vive ? c.pal.selection : NkColor(20, 24, 30, 255), 1.5f, 1.5f);
				}
			}
		}

		bool NkEditeurPoigneesCollisionSouris(NkEditeurCadre &c, const nkgui::NkRect &aire) {
			NkEditeurInterface &ui = c.ui;
			NkTransform2D *t = nullptr;
			NkCollisionneur2D *col = nullptr;
			if (!Actives(c, t, col)) {
				ui.poigneeTenue = -1;
				ui.poigneeSurvol = -1;
				return false;
			}
			NkEditeurModele &m = c.m;
			const nkgui::NkGuiInput &in = c.ctx.input;
			const NkVue2D &cam = m.scene.Camera();
			const NkVec2f souris(in.mousePos.x, in.mousePos.y);
			const NkVec2f monde = cam.EcranVersMonde(souris);
			const ecs::NkEntityId id = m.selection;

			if (ui.poigneeTenue < 0) {
				// Le survol : la poignee la plus proche sous le curseur.
				ui.poigneeSurvol = -1;
				if (!NkEditeurDans(aire, in.mousePos) || NkEditeurDans(ui.barreFlottante, in.mousePos)) {
					return false;
				}
				NkPoignee p[96];
				const uint32 np = Poignees(*col, 1.f / cam.Zoom(), p, 96u);
				float32 meilleure = RAYON_PRISE * RAYON_PRISE;
				for (uint32 i = 0; i < np; ++i) {
					const NkVec2f e = cam.MondeVersEcran(NkPointCollision2D(*t, *col, p[i].local));
					const float32 d = (e.x - souris.x) * (e.x - souris.x) + (e.y - souris.y) * (e.y - souris.y);
					// Un sommet passe avant un « + » qui le chevaucherait.
					const float32 dPondere = p[i].id >= P_MILIEU ? d * 1.5f : d;
					if (dPondere < meilleure) {
						meilleure = dPondere;
						ui.poigneeSurvol = p[i].id;
					}
				}
				if (ui.poigneeSurvol < 0 || !in.mouseClicked[0]) {
					return false;
				}
				// L'APPUI sur une poignee : le geste commence, annulable.
				NkEditeurRetenir(m);
				t = m.scene.Monde().Get<NkTransform2D>(id);
				col = m.scene.Monde().Get<NkCollisionneur2D>(id);
				const int32 cible = ui.poigneeSurvol;
				const uint32 ns = NkNbSommetsCollision2D(*col);
				const uint32 min = col->forme == NkForme2D::NK_POLYGONE ? 3u : 2u;
				if (cible >= P_SOMMET && cible < P_MILIEU && in.ctrlDown) {
					// Ctrl+clic : le sommet s'en va.
					if (ns > min) {
						for (uint32 i = static_cast<uint32>(cible - P_SOMMET); i + 1u < ns; ++i) {
							col->sommets[i] = col->sommets[i + 1u];
						}
						col->nbSommets = static_cast<uint8>(ns - 1u);
						NkEditeurAnnoncer(m, "Sommet retiré (Ctrl+Z pour le rendre)");
					} else {
						NkEditeurAnnoncer(m, "Un polygone garde 3 sommets, une chaîne 2");
					}
					if (m.scene.Monde().Has<NkCorps2D>(id)) {
						m.scene.ActualiserCorps(id);
					}
					ui.poigneeSurvol = -1;
					return true;
				}
				if (cible >= P_MILIEU) {
					// Le « + » : un sommet nait au milieu du cote, et se tire aussitot.
					const uint32 apres = static_cast<uint32>(cible - P_MILIEU);
					const NkVec2f a = col->sommets[apres];
					const NkVec2f b = col->sommets[(apres + 1u) % ns];
					for (uint32 i = ns; i > apres + 1u; --i) {
						col->sommets[i] = col->sommets[i - 1u];
					}
					col->sommets[apres + 1u] = NkVec2f((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
					col->nbSommets = static_cast<uint8>(ns + 1u);
					ui.poigneeTenue = P_SOMMET + static_cast<int32>(apres + 1u);
				} else {
					ui.poigneeTenue = cible;
				}
				ui.poigneeLocal0 = VersLocal(*t, *col, monde);
				if (NkRenduForme2D *f = m.scene.Monde().Get<NkRenduForme2D>(id)) {
					f->collisionSuit = false; // regle a la main : la forme ne le refait plus
				}
				return true;
			}

			// ── Le geste en cours ──────────────────────────────────────────
			if (!in.mouseDown[0]) {
				ui.poigneeTenue = -1;
				return true;
			}
			const int32 p = ui.poigneeTenue;
			const NkVec2f l = VersLocal(*t, *col, monde);
			const float32 accroche = ui.accrocheGrille != in.ctrlDown ? ui.pasGrille : 0.f;
			auto Accrocher = [accroche](float32 v) { return accroche > 0.f ? NkEditeurAccrocher(v, accroche) : v; };
			if (p == P_CENTRE) {
				const NkVec2f e = VersEntite(*t, monde);
				col->decalage = NkVec2f(Accrocher(e.x), Accrocher(e.y));
			} else if (p == P_ROTATION) {
				const NkVec2f e = VersEntite(*t, monde);
				float32 a = std::atan2(e.y - col->decalage.y, e.x - col->decalage.x) - 1.5707963f;
				if (ui.accrocheAngle != in.ctrlDown && ui.pasAngle > 0.f) {
					const float32 pas = ui.pasAngle / 57.2957795f;
					a = NkEditeurAccrocher(a, pas);
				}
				col->rotation = a;
			} else if (p >= P_COIN && p < P_COIN + 4) {
				col->demiTaille = NkVec2f(math::NkMax(Accrocher(math::NkAbs(l.x)), 0.02f), math::NkMax(Accrocher(math::NkAbs(l.y)), 0.02f));
			} else if (p == P_RAYON) {
				col->rayon = math::NkMax(Accrocher(std::sqrt(l.x * l.x + l.y * l.y)), 0.02f);
			} else if (p == P_BOUT || p == P_BOUT + 1) {
				col->demiTaille.x = math::NkMax(Accrocher(math::NkAbs(l.x)) - col->rayon, 0.f);
			} else if (p == P_FLANC) {
				col->rayon = math::NkMax(Accrocher(math::NkAbs(l.y)), 0.02f);
			} else if (p >= P_SOMMET && p < P_MILIEU) {
				const uint32 i = static_cast<uint32>(p - P_SOMMET);
				if (i < NkNbSommetsCollision2D(*col)) {
					col->sommets[i] = NkVec2f(Accrocher(l.x), Accrocher(l.y));
				}
			}
			if (m.scene.Monde().Has<NkCorps2D>(id)) {
				m.scene.ActualiserCorps(id); // la forme vit aussi dans le solveur
			}
			return true;
		}

	} // namespace editeur
} // namespace nkentseu
