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
// Et les POINTS d'une forme LIGNE ou POLYGONE LIBRE dont le collisionneur suit
// la forme : ce sont eux qu'on tire (memes gestes), le collisionneur suit.
// Chaque geste est annulable (Ctrl+Z) ; un corps rigide est refait a chaque
// pas du geste (ActualiserCorps). L'AIMANT (NkEditeurAimant.cpp) colle un
// point tire aux sommets, aretes et faces des autres objets.
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
			constexpr int32 P_COIN = 10;		 ///< + 0..3
			constexpr int32 P_RAYON = 20;
			constexpr int32 P_BOUT = 30;		 ///< + 0..1
			constexpr int32 P_FLANC = 32;
			constexpr int32 P_SOMMET = 100;		 ///< + i
			constexpr int32 P_MILIEU = 200;		 ///< + i : inserer apres le sommet i
			constexpr int32 P_POINT = 300;		 ///< + i : un point de la FORME
			constexpr int32 P_POINT_MILIEU = 400; ///< + i : inserer un point apres le point i
			constexpr float32 RAYON_PRISE = 8.f; ///< px

			struct NkPoignee {
					int32 id;
					NkVec2f local; ///< repere du collisionneur, ou de l'ENTITE (points d'une forme)
			};

			/// Ce que les poignees editent : le collisionneur, ou les POINTS d'une
			/// ligne / d'un polygone libre dont le collisionneur suit la forme.
			struct NkCible {
					NkTransform2D *t = nullptr;
					NkCollisionneur2D *col = nullptr;
					NkRenduForme2D *forme = nullptr; ///< non nul : on edite ses points
			};

			bool EditePoints(const NkRenduForme2D *f) {
				return f != nullptr && f->visible && f->collisionSuit &&
					   (f->genre == NkGenreForme2D::NK_LIGNE || f->genre == NkGenreForme2D::NK_POLYGONE_LIBRE);
			}

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

			/// Monde -> repere de la FORME (l'entite, echelle comprise : l'inverse de
			/// NkTransform2D::VersMonde).
			NkVec2f VersForme(const NkTransform2D &t, const NkVec2f &w) {
				const NkVec2f e = VersEntite(t, w);
				const float32 ex = std::fabs(t.echelle.x) > 1.0e-6f ? t.echelle.x : 1.0e-6f;
				const float32 ey = std::fabs(t.echelle.y) > 1.0e-6f ? t.echelle.y : 1.0e-6f;
				return NkVec2f(e.x / ex, e.y / ey);
			}

			NkVec2f EnMonde(const NkCible &k, const NkPoignee &p) {
				if (p.id >= P_POINT) {
					return k.t->VersMonde(p.local);
				}
				return NkPointCollision2D(*k.t, *k.col, p.local);
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

			/// Les poignees de la cible, et leur nombre. La poignee de rotation est a
			/// 28 px au-dessus de la forme (`pixel` m par px).
			uint32 Poignees(const NkCible &k, float32 pixel, NkPoignee *p, uint32 cap) {
				uint32 n = 0;
				auto A = [&](int32 id, const NkVec2f &l) {
					if (n < cap) {
						p[n++] = NkPoignee{id, l};
					}
				};
				if (k.forme != nullptr) {
					const NkRenduForme2D &f = *k.forme;
					const bool ferme = f.genre == NkGenreForme2D::NK_POLYGONE_LIBRE;
					const uint32 np = f.nbPoints < NK_FORME_POINTS_MAX ? f.nbPoints : NK_FORME_POINTS_MAX;
					for (uint32 i = 0; i < np; ++i) {
						A(P_POINT + static_cast<int32>(i), f.points[i]);
					}
					if (np < NK_FORME_POINTS_MAX) {
						const uint32 nbCotes = ferme ? np : (np > 0u ? np - 1u : 0u);
						for (uint32 i = 0; i < nbCotes; ++i) {
							const NkVec2f &a = f.points[i];
							const NkVec2f &b = f.points[(i + 1u) % np];
							A(P_POINT_MILIEU + static_cast<int32>(i), NkVec2f((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f));
						}
					}
					return n;
				}
				const NkCollisionneur2D &c = *k.col;
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

			bool Actives(NkEditeurCadre &c, NkCible &k) {
				NkEditeurModele &m = c.m;
				if (!c.ui.editionCollision || m.etat != NkEtatJeu::NK_EDITION || !m.aSelection || !m.scene.Monde().IsAlive(m.selection)) {
					return false;
				}
				k.t = m.scene.Monde().Get<NkTransform2D>(m.selection);
				k.col = m.scene.Monde().Get<NkCollisionneur2D>(m.selection);
				NkRenduForme2D *f = m.scene.Monde().Get<NkRenduForme2D>(m.selection);
				k.forme = EditePoints(f) ? f : nullptr;
				return k.t != nullptr && (k.col != nullptr || k.forme != nullptr);
			}

			/// Apres un changement : la forme refait son collisionneur ; un corps
			/// rigide est refait (la forme vit aussi dans le solveur).
			void Appliquer(NkEditeurModele &m, ecs::NkEntityId id, bool forme) {
				if (forme) {
					NkEditeurFormeChangee(m, id);
					return;
				}
				if (m.scene.Monde().Has<NkCorps2D>(id)) {
					m.scene.ActualiserCorps(id);
				}
			}
		} // namespace

		// =====================================================================
		void NkEditeurDessinerPoigneesCollision(NkEditeurCadre &c, nkgui::NkGuiDrawList &dl) {
			// L'indicateur de l'AIMANT, qu'on edite ou qu'on deplace un bloc.
			NkEditeurDessinerAimant(c, dl);
			NkCible k;
			if (!Actives(c, k)) {
				return;
			}
			const NkVue2D &cam = c.m.scene.Camera();
			const NkColor vert(0, 224, 122, 255);
			const NkColor bleu(90, 170, 255, 255);
			// Le contour edite, plus epais que la surcouche.
			if (k.col != nullptr) {
				NkVec2f pts[NK_COLLISION_SOMMETS_MAX + 72u];
				bool ferme = true;
				const uint32 n = NkContourCollisionneur2D(*k.t, *k.col, pts, NK_COLLISION_SOMMETS_MAX + 72u, ferme);
				for (uint32 i = 0; i < n; ++i) {
					pts[i] = cam.MondeVersEcran(pts[i]);
				}
				if (n >= 2u) {
					dl.AddPolyline(pts, static_cast<int32>(n), vert, k.forme != nullptr ? 1.5f : 2.5f, ferme);
				}
			}
			NkPoignee p[96];
			const uint32 np = Poignees(k, 1.f / cam.Zoom(), p, 96u);
			const NkVec2f centre = k.col != nullptr ? cam.MondeVersEcran(NkPointCollision2D(*k.t, *k.col, NkVec2f(0.f, 0.f))) : NkVec2f(0.f, 0.f);
			for (uint32 i = 0; i < np; ++i) {
				const NkVec2f e = cam.MondeVersEcran(EnMonde(k, p[i]));
				const bool vive = c.ui.poigneeTenue == p[i].id || c.ui.poigneeSurvol == p[i].id;
				const float32 r = vive ? 6.5f : 5.f;
				const NkColor teinte = p[i].id >= P_POINT ? bleu : vert;
				if (p[i].id == P_ROTATION) {
					dl.AddLine(centre, e, NkColor(0, 224, 122, 140), 1.f);
					dl.AddCircleFilled(e, r, vive ? c.pal.selection : vert);
					dl.AddCircle(e, r, NkColor(20, 24, 30, 255), 1.2f);
				} else if ((p[i].id >= P_MILIEU && p[i].id < P_POINT) || p[i].id >= P_POINT_MILIEU) {
					// Le « + » d'un milieu de cote.
					dl.AddCircleFilled(e, r - 1.f, NkColor(20, 24, 30, 200));
					dl.AddLine(NkVec2{e.x - 3.f, e.y}, NkVec2{e.x + 3.f, e.y}, vive ? c.pal.selection : teinte, 1.5f);
					dl.AddLine(NkVec2{e.x, e.y - 3.f}, NkVec2{e.x, e.y + 3.f}, vive ? c.pal.selection : teinte, 1.5f);
				} else {
					const NkRect q{e.x - r, e.y - r, 2.f * r, 2.f * r};
					dl.AddRectFilled(q, p[i].id == P_CENTRE ? vert : (p[i].id >= P_POINT ? bleu : NkColor(245, 245, 245, 255)), 1.5f);
					dl.AddRect(q, vive ? c.pal.selection : NkColor(20, 24, 30, 255), 1.5f, 1.5f);
				}
			}
		}

		bool NkEditeurPoigneesCollisionSouris(NkEditeurCadre &c, const nkgui::NkRect &aire) {
			NkEditeurInterface &ui = c.ui;
			NkCible k;
			if (!Actives(c, k)) {
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
				const uint32 np = Poignees(k, 1.f / cam.Zoom(), p, 96u);
				float32 meilleure = RAYON_PRISE * RAYON_PRISE;
				for (uint32 i = 0; i < np; ++i) {
					const NkVec2f e = cam.MondeVersEcran(EnMonde(k, p[i]));
					const float32 d = (e.x - souris.x) * (e.x - souris.x) + (e.y - souris.y) * (e.y - souris.y);
					// Un sommet passe avant un « + » qui le chevaucherait.
					const bool plus = (p[i].id >= P_MILIEU && p[i].id < P_POINT) || p[i].id >= P_POINT_MILIEU;
					const float32 dPondere = plus ? d * 1.5f : d;
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
				Actives(c, k); // les pointeurs, apres la photo
				const int32 cible = ui.poigneeSurvol;
				// Les sommets (collisionneur) ou les points (forme) : meme geste.
				const bool forme = cible >= P_POINT;
				NkVec2f *liste = forme ? k.forme->points : k.col->sommets;
				uint8 &nb = forme ? k.forme->nbPoints : k.col->nbSommets;
				const uint32 ns = forme ? (k.forme->nbPoints < NK_FORME_POINTS_MAX ? k.forme->nbPoints : NK_FORME_POINTS_MAX)
										: NkNbSommetsCollision2D(*k.col);
				const uint32 min = forme ? (k.forme->genre == NkGenreForme2D::NK_LIGNE ? 2u : 3u)
										 : (k.col->forme == NkForme2D::NK_POLYGONE ? 3u : 2u);
				const int32 base = forme ? P_POINT : P_SOMMET;
				const int32 baseMilieu = forme ? P_POINT_MILIEU : P_MILIEU;
				const bool estSommet = cible >= base && cible < base + 100;
				if (estSommet && in.ctrlDown) {
					// Ctrl+clic : le sommet (ou le point) s'en va.
					if (ns > min) {
						for (uint32 i = static_cast<uint32>(cible - base); i + 1u < ns; ++i) {
							liste[i] = liste[i + 1u];
						}
						nb = static_cast<uint8>(ns - 1u);
						NkEditeurAnnoncer(m, "Sommet retiré (Ctrl+Z pour le rendre)");
					} else {
						NkEditeurAnnoncer(m, "Un polygone garde 3 sommets, une chaîne ou une ligne 2");
					}
					Appliquer(m, id, forme);
					ui.poigneeSurvol = -1;
					return true;
				}
				if (cible >= baseMilieu && cible < baseMilieu + 100) {
					// Le « + » : un sommet nait au milieu du cote, et se tire aussitot.
					const uint32 apres = static_cast<uint32>(cible - baseMilieu);
					const NkVec2f a = liste[apres];
					const NkVec2f b = liste[(apres + 1u) % ns];
					for (uint32 i = ns; i > apres + 1u; --i) {
						liste[i] = liste[i - 1u];
					}
					liste[apres + 1u] = NkVec2f((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
					nb = static_cast<uint8>(ns + 1u);
					ui.poigneeTenue = base + static_cast<int32>(apres + 1u);
					Appliquer(m, id, forme);
				} else {
					ui.poigneeTenue = cible;
				}
				if (!forme) {
					if (NkRenduForme2D *f = m.scene.Monde().Get<NkRenduForme2D>(id)) {
						f->collisionSuit = false; // regle a la main : la forme ne le refait plus
					}
				}
				return true;
			}

			// ── Le geste en cours ──────────────────────────────────────────
			if (!in.mouseDown[0]) {
				ui.poigneeTenue = -1;
				return true;
			}
			const int32 p = ui.poigneeTenue;
			// L'AIMANT colle le point tire aux AUTRES objets (pas la rotation : un
			// angle ne se colle pas a un sommet). Un accroche l'emporte sur la grille.
			const NkVec2f tire = p != P_ROTATION ? NkEditeurAimanterPoignee(c, monde, id) : monde;
			const bool colle = tire.x != monde.x || tire.y != monde.y;
			const float32 accroche = !colle && ui.accrocheGrille != in.ctrlDown ? ui.pasGrille : 0.f;
			auto Accrocher = [accroche](float32 v) { return accroche > 0.f ? NkEditeurAccrocher(v, accroche) : v; };
			if (p >= P_POINT && p < P_POINT_MILIEU) {
				const uint32 i = static_cast<uint32>(p - P_POINT);
				if (k.forme != nullptr && i < k.forme->nbPoints) {
					const NkVec2f l = VersForme(*k.t, tire);
					k.forme->points[i] = colle ? l : NkVec2f(Accrocher(l.x), Accrocher(l.y));
					Appliquer(m, id, true);
				}
				return true;
			}
			if (k.col == nullptr) {
				return true;
			}
			NkCollisionneur2D *col = k.col;
			const NkVec2f l = VersLocal(*k.t, *col, tire);
			if (p == P_CENTRE) {
				const NkVec2f e = VersEntite(*k.t, tire);
				col->decalage = NkVec2f(Accrocher(e.x), Accrocher(e.y));
			} else if (p == P_ROTATION) {
				const NkVec2f e = VersEntite(*k.t, monde);
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
			Appliquer(m, id, false);
			return true;
		}

	} // namespace editeur
} // namespace nkentseu
