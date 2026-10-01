//
// NkEditeurVue.cpp
// =============================================================================
// Description :
//   La colonne centrale : la BARRE DE VUE (Cadrer, Grille, Collisionneurs ;
//   l'appareil et le zoom a droite) et le VISEUR 2D, avec sa souris, ses
//   GIZMOS, son repere d'axes, son cadrage anime et -- dans son coin haut
//   droit -- la BARRE FLOTTANTE d'UE5 (Q / W / E / R, repere local ou monde,
//   accrochage de la grille, des angles et de l'echelle, chacun avec son pas).
//
// Caracteristiques :
//   - Le dessin de la scene est celui de NkEditeurViseur.cpp : ce fichier lui
//     donne une aire, y route la souris et peint PAR-DESSUS ce qui est de
//     l'editeur (gizmos, repere).
//   - Les gestes (2026-09-29, a la maniere d'UE5 applique a la 2D) :
//       molette               zoom SOUS le curseur
//       molette PRESSEE       deplacer la vue
//       clic gauche           choisir ce qui est sous le curseur ; le vide
//                             deselectionne
//       clic gauche + glisser deplacer l'entite, au-dela de 4 px (en deca,
//                             c'etait un clic) -- en EDITION seulement
//       clic droit            menu contextuel (entite, ou vide)
//       W / E / R             gizmo Deplacer / Tourner / Echelle sur la
//                             selection ; Ctrl inverse l'accrochage
//   - Chaque geste appelle une action de NkEditeurActions.h : le banc
//     (`--selftest`) eprouve ce que fait chaque poignee, sans fenetre.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurLumiere.h"
#include "Editeur/NkEditeurPlacer.h"
#include "Editeur/NkEditeurViseur.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/NkThemeToGui.h"

#include <cmath>

namespace nkentseu {
	namespace editeur {

		using editorkit::NkRole;
		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {

			constexpr float32 SEUIL_GLISSER = 4.f;	///< px : en deca, c'est un clic
			constexpr float32 GIZMO_AXE = 72.f;		///< longueur d'une fleche (px)
			constexpr float32 GIZMO_ANNEAU = 60.f;	///< rayon de l'anneau de rotation (px)
			constexpr float32 GIZMO_PRISE = 7.f;	///< tolerance de saisie d'une poignee (px)
			constexpr float32 CADRAGE_DUREE = 0.28f; ///< s : le trajet de la camera (F)
			constexpr float32 PI = 3.14159265f;

			enum : int32 { POIGNEE_AUCUNE = 0, POIGNEE_X = 1, POIGNEE_Y = 2, POIGNEE_XY = 3, POIGNEE_ANNEAU = 4 };

			NkColor Role(NkEditeurCadre &c, NkRole r) {
				return editorkit::NkThemeUnpack(c.theme.Get(r));
			}

			float32 Distance(const NkVec2f &a, const NkVec2f &b) noexcept {
				const float32 dx = a.x - b.x;
				const float32 dy = a.y - b.y;
				return math::NkSqrt(dx * dx + dy * dy);
			}

			/// Distance d'un point a un segment, en pixels.
			float32 DistanceSegment(const NkVec2f &p, const NkVec2f &a, const NkVec2f &b) noexcept {
				const float32 vx = b.x - a.x;
				const float32 vy = b.y - a.y;
				const float32 l2 = vx * vx + vy * vy;
				float32 t = l2 > 0.f ? ((p.x - a.x) * vx + (p.y - a.y) * vy) / l2 : 0.f;
				t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				return Distance(p, NkVec2f(a.x + vx * t, a.y + vy * t));
			}

			bool Dans(const NkRect &r, const NkVec2f &p) noexcept {
				return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
			}

			/// L'accrochage de CE geste : son reglage, inverse tant que Ctrl est tenu.
			bool Accroche(NkEditeurCadre &c, bool reglage) noexcept {
				return reglage != c.ctx.input.ctrlDown;
			}

			NkVec2f Plus(const NkVec2f &a, const NkVec2f &b, float32 k) noexcept {
				return NkVec2f(a.x + b.x * k, a.y + b.y * k);
			}

			float32 Scalaire(const NkVec2f &a, const NkVec2f &b) noexcept {
				return a.x * b.x + a.y * b.y;
			}

			/// Angle ramene dans ]-pi, pi].
			float32 Replier(float32 a) noexcept {
				while (a > PI) {
					a -= 2.f * PI;
				}
				while (a <= -PI) {
					a += 2.f * PI;
				}
				return a;
			}

			// =================================================================
			// LA BARRE DE VUE
			// =================================================================
			/// Un groupe a gauche, les reperes a droite (UI_SPEC §2.2, regle 3).
			/// Ce qui s'y lit est VRAI en continu : zoom, appareil et pas
			/// d'accrochage sont relus a chaque trame.
			void BarreVue(NkEditeurCadre &c) {
				const NkRect &b = c.ui.barreVue;
				auto &dl = c.ctx.dl;
				dl.AddRectFilled(b, c.pal.entete);
				dl.AddRectFilled(NkRect{b.x, b.y + b.h - 1.f, b.w, 1.f}, c.pal.bord);
				float32 x = b.x + 6.f;
				struct NkBouton {
						NkString texte;
						int32 action;
						bool enfonce;
				};
				// L'accrochage n'est plus ici : il est dans la barre flottante du
				// viseur, avec un interrupteur et un pas PAR GESTE (UE5).
				const NkBouton boutons[3] = {
					{NkString("Cadrer"), NK_A_CADRER_SELECTION, false},
					{NkString("Grille"), NK_A_GRILLE, c.m.voirGrille},
					{NkString("Collisionneurs"), NK_A_COLLISIONNEURS, c.m.voirCollisionneurs},
				};
				for (int32 i = 0; i < 3; ++i) {
					const float32 w = renderer::NkTexteLargeur(c.petite, boutons[i].texte.CStr()) + 16.f;
					const NkRect r{x, b.y + 4.f, w, b.h - 8.f};
					const bool clic = NkEditeurBouton(c, r, "", boutons[i].enfonce);
					renderer::NkTexteDansBoite(dl, c.petite, r, boutons[i].texte.CStr(),
											   boutons[i].enfonce ? c.pal.surAccent : c.pal.texte);
					if (clic) {
						NkEditeurExecuter(c, boutons[i].action);
					}
					x += w + 3.f;
				}

				// A droite : ce que la vue represente. L'outil arme y figure parce
				// que c'est lui qui decide de ce que fera le prochain clic.
				const NkProfilAppareil p = c.m.ProfilCourant();
				NkString arme;
				if (c.m.outil == NkOutil::NK_POSER) {
					arme = NkString::Format("pose : %s   ·   ",
											c.m.acteurSimple ? "entité simple" : NkActeurSimInfo(c.m.acteur).nom);
				}
				const NkString reperes = NkString::Format("%s%s %ux%u   ·   %.0f px/m", arme.CStr(), p.nom, p.largeur,
														  p.hauteur, static_cast<double>(c.m.scene.Camera().Zoom()));
				const float32 ty = b.y + (b.h - renderer::NkTexteHauteurLigne(c.petite, 12.f)) * 0.5f;
				if (renderer::NkTexteLargeur(c.petite, reperes.CStr()) < b.x + b.w - x - 16.f) {
					renderer::NkTexteADroite(dl, c.petite, b.x + b.w - 8.f, ty, reperes.CStr(), c.pal.attenue);
				}
			}

			// =================================================================
			// LES GIZMOS
			// =================================================================
			/// Un gizmo se montre sur la selection, avec l'outil du meme nom, et
			/// seulement en EDITION : en jeu, la simulation possede les positions
			/// (meme regle que le glisser, NkEditeurPeutDeplacer). Ni sur une
			/// entite VERROUILLEE (le cadenas interdit de la bouger dans la vue),
			/// ni sur une entite CACHEE (l'oeil ferme ne laisse rien a viser).
			bool GizmoVisible(NkEditeurCadre &c) {
				const NkOutil o = c.m.outil;
				const bool mode = o == NkOutil::NK_DEPLACER || o == NkOutil::NK_TOURNER || o == NkOutil::NK_ECHELLE;
				return mode && c.m.aSelection && c.m.scene.Monde().IsAlive(c.m.selection) && NkEditeurPeutDeplacer(c.m) &&
					   !NkEditeurVerrouilleDansLaVue(c.m, c.m.selection) && !NkEditeurCacheDansLaVue(c.m, c.m.selection);
			}

			/// Le centre du gizmo, a l'ecran : le centre de l'entite.
			bool CentreGizmo(NkEditeurCadre &c, NkVec2f &o) {
				NkVec2f centre;
				if (!NkEditeurCentreSelection(c.m, centre)) {
					return false;
				}
				o = c.m.scene.Camera().MondeVersEcran(centre);
				return true;
			}

			/// La rotation de l'entite choisie, pour ses axes LOCAUX. La matiere n'a
			/// pas d'orientation (ses particules tournent chacune) : 0.
			float32 RotationSelection(NkEditeurCadre &c) {
				if (c.m.scene.Monde().Has<NkCorpsMou2D>(c.m.selection)) {
					return 0.f;
				}
				const NkTransform2D *t = c.m.scene.Monde().Get<NkTransform2D>(c.m.selection);
				return t != nullptr ? t->rotation : 0.f;
			}

			/// Les deux axes du gizmo, en MONDE (unitaires, Y vers le haut) : ceux
			/// du monde, ou ceux de l'entite. Deplacer suit le reglage local /
			/// monde ; l'ECHELLE est toujours locale (elle agit sur les dimensions
			/// propres de l'entite : l'afficher sur les axes du monde mentirait des
			/// qu'elle est tournee).
			void AxesMonde(NkEditeurCadre &c, float32 rotation, NkVec2f &ax, NkVec2f &ay) {
				const bool local = c.m.outil == NkOutil::NK_ECHELLE || (c.m.outil == NkOutil::NK_DEPLACER && c.ui.repereLocal);
				const float32 a = local ? rotation : 0.f;
				ax = NkVec2f(std::cos(a), std::sin(a));
				ay = NkVec2f(-std::sin(a), std::cos(a));
			}

			/// Les memes, a l'ECRAN : l'ecran descend quand le monde monte.
			void AxesEcran(NkEditeurCadre &c, float32 rotation, NkVec2f &ax, NkVec2f &ay) {
				NkVec2f mx, my;
				AxesMonde(c, rotation, mx, my);
				ax = NkVec2f(mx.x, -mx.y);
				ay = NkVec2f(my.x, -my.y);
			}

			/// Le carre du plan XY (deplacer) ou de l'echelle uniforme, dans le
			/// repere des axes : [u0, u1] x [u0, u1].
			void CarreGizmo(NkOutil outil, float32 &u0, float32 &u1) noexcept {
				if (outil == NkOutil::NK_ECHELLE) {
					u0 = -7.f;
					u1 = 7.f;
					return;
				}
				u0 = 12.f;
				u1 = 28.f;
			}

			/// La poignee sous `p`. Le carre d'abord : il est POSE sur les fleches,
			/// et le viser doit l'emporter.
			int32 PoigneeSous(const NkVec2f &o, NkOutil outil, const NkVec2f &p, const NkVec2f &ax, const NkVec2f &ay) noexcept {
				if (outil == NkOutil::NK_TOURNER) {
					return math::NkAbs(Distance(p, o) - GIZMO_ANNEAU) < GIZMO_PRISE ? POIGNEE_ANNEAU : POIGNEE_AUCUNE;
				}
				const NkVec2f d(p.x - o.x, p.y - o.y);
				const float32 u = Scalaire(d, ax);
				const float32 v = Scalaire(d, ay);
				float32 u0 = 0.f, u1 = 0.f;
				CarreGizmo(outil, u0, u1);
				if (u >= u0 && u <= u1 && v >= u0 && v <= u1) {
					return POIGNEE_XY;
				}
				if (DistanceSegment(p, Plus(o, ax, 10.f), Plus(o, ax, GIZMO_AXE + 6.f)) < GIZMO_PRISE) {
					return POIGNEE_X;
				}
				if (DistanceSegment(p, Plus(o, ay, 10.f), Plus(o, ay, GIZMO_AXE + 6.f)) < GIZMO_PRISE) {
					return POIGNEE_Y;
				}
				return POIGNEE_AUCUNE;
			}

			void DessinerGizmo(NkEditeurCadre &c, nkgui::NkGuiDrawList &dl) {
				NkEditeurInterface &ui = c.ui;
				if (!GizmoVisible(c)) {
					return;
				}
				NkVec2f o;
				if (!CentreGizmo(c, o)) {
					return;
				}
				const NkOutil outil = c.m.outil;
				const int32 vive = ui.gizmoTenu != POIGNEE_AUCUNE ? ui.gizmoTenu : ui.gizmoSurvol;
				// X ROUGE, Y VERT (roles AxisX / AxisY du theme) ; la poignee tenue
				// ou survolee passe a l'AMBRE, la couleur de la selection.
				const NkColor ambre = Role(c, NkRole::AccentSel);
				const NkColor cx = vive == POIGNEE_X ? ambre : Role(c, NkRole::AxisX);
				const NkColor cy = vive == POIGNEE_Y ? ambre : Role(c, NkRole::AxisY);
				if (outil == NkOutil::NK_TOURNER) {
					const NkColor ca = vive == POIGNEE_ANNEAU ? ambre : Role(c, NkRole::AxisZ);
					dl.AddCircle(NkVec2{o.x, o.y}, GIZMO_ANNEAU, ca, 2.5f, 64);
					if (ui.gizmoTenu == POIGNEE_ANNEAU) {
						// Le rayon de depart et le rayon courant, et l'angle en degres :
						// on voit de combien on tourne pendant qu'on tourne.
						const float32 a0 = std::atan2(ui.gizmoMonde0.y - ui.gizmoCentre0.y, ui.gizmoMonde0.x - ui.gizmoCentre0.x);
						const float32 a1 = a0 + ui.gizmoAngle;
						dl.AddLine(NkVec2{o.x, o.y}, NkVec2{o.x + std::cos(a0) * GIZMO_ANNEAU, o.y - std::sin(a0) * GIZMO_ANNEAU},
								   c.pal.attenue, 1.f);
						dl.AddLine(NkVec2{o.x, o.y}, NkVec2{o.x + std::cos(a1) * GIZMO_ANNEAU, o.y - std::sin(a1) * GIZMO_ANNEAU},
								   ambre, 2.f);
						const NkString deg = NkString::Format("%+.1f°", static_cast<double>(ui.gizmoAngle * 180.f / PI));
						renderer::NkTexte(dl, c.petite, o.x + GIZMO_ANNEAU + 8.f, o.y - 8.f, deg.CStr(), ambre);
					}
					dl.AddCircleFilled(NkVec2{o.x, o.y}, 3.f, ca);
					return;
				}
				// Les axes : ceux de la saisie tant qu'une poignee est tenue (ils ne
				// tournent pas sous la main), ceux de l'entite sinon.
				NkVec2f ax, ay;
				AxesEcran(c, ui.gizmoTenu != POIGNEE_AUCUNE ? ui.gizmoRot0 : RotationSelection(c), ax, ay);
				const NkVec2f finX = Plus(o, ax, GIZMO_AXE);
				const NkVec2f finY = Plus(o, ay, GIZMO_AXE);
				dl.AddLine(NkVec2{o.x, o.y}, NkVec2{finX.x, finX.y}, cx, 2.5f);
				dl.AddLine(NkVec2{o.x, o.y}, NkVec2{finY.x, finY.y}, cy, 2.5f);
				auto V = [](const NkVec2f &p) { return NkVec2{p.x, p.y}; };
				if (outil == NkOutil::NK_DEPLACER) {
					// Des FLECHES : on tire dans leur direction.
					dl.AddTriangleFilled(V(Plus(finX, ax, 12.f)), V(Plus(finX, ay, 6.f)), V(Plus(finX, ay, -6.f)), cx);
					dl.AddTriangleFilled(V(Plus(finY, ay, 12.f)), V(Plus(finY, ax, -6.f)), V(Plus(finY, ax, 6.f)), cy);
				} else {
					// Des CUBES au bout : on etire.
					for (int32 k = 0; k < 2; ++k) {
						const NkVec2f f = k == 0 ? finX : finY;
						const NkColor col = k == 0 ? cx : cy;
						const NkVec2f a = Plus(Plus(f, ax, -5.f), ay, -5.f);
						const NkVec2f b = Plus(Plus(f, ax, 5.f), ay, -5.f);
						const NkVec2f d = Plus(Plus(f, ax, 5.f), ay, 5.f);
						const NkVec2f e = Plus(Plus(f, ax, -5.f), ay, 5.f);
						dl.AddTriangleFilled(V(a), V(b), V(d), col);
						dl.AddTriangleFilled(V(a), V(d), V(e), col);
					}
				}
				float32 u0 = 0.f, u1 = 0.f;
				CarreGizmo(outil, u0, u1);
				const NkVec2 carre[4] = {V(Plus(Plus(o, ax, u0), ay, u0)), V(Plus(Plus(o, ax, u1), ay, u0)),
										 V(Plus(Plus(o, ax, u1), ay, u1)), V(Plus(Plus(o, ax, u0), ay, u1))};
				const bool carreVif = vive == POIGNEE_XY;
				NkColor fond = carreVif ? ambre : Role(c, NkRole::AxisZ);
				fond.a = carreVif ? 200 : 110;
				dl.AddTriangleFilled(carre[0], carre[1], carre[2], fond);
				dl.AddTriangleFilled(carre[0], carre[2], carre[3], fond);
				dl.AddPolyline(carre, 4, carreVif ? ambre : c.pal.texte, 1.f, true);
			}

			/// Le gizmo et la souris. Rend true si le gizmo a PRIS le geste : le
			/// reste de la souris du viseur ne doit alors rien en faire.
			bool GizmoSouris(NkEditeurCadre &c, const NkRect &aire) {
				NkEditeurInterface &ui = c.ui;
				NkEditeurModele &m = c.m;
				const nkgui::NkGuiInput &in = c.ctx.input;
				const NkVec2f p(in.mousePos.x, in.mousePos.y);
				if (ui.gizmoTenu == POIGNEE_AUCUNE) {
					ui.gizmoSurvol = POIGNEE_AUCUNE;
					NkVec2f o;
					if (!GizmoVisible(c) || !Dans(aire, p) || Dans(ui.barreFlottante, p) || !CentreGizmo(c, o)) {
						return false;
					}
					NkVec2f ax, ay;
					AxesEcran(c, RotationSelection(c), ax, ay);
					ui.gizmoSurvol = PoigneeSous(o, m.outil, p, ax, ay);
					if (ui.gizmoSurvol == POIGNEE_AUCUNE || !in.mouseClicked[0]) {
						return false;
					}
					ui.gizmoTenu = ui.gizmoSurvol;
					ui.gizmoRot0 = RotationSelection(c);
					NkEditeurCentreSelection(m, ui.gizmoCentre0);
					ui.gizmoMonde0 = m.scene.Camera().EcranVersMonde(p);
					ui.gizmoEcran0 = p;
					ui.gizmoAngle = 0.f;
					ui.gizmoEchelle = NkVec2f(1.f, 1.f);
					return true;
				}
				if (!in.mouseDown[0] || !m.aSelection) {
					ui.gizmoTenu = POIGNEE_AUCUNE;
					return true;
				}
				const NkVec2f monde = m.scene.Camera().EcranVersMonde(p);
				switch (m.outil) {
					case NkOutil::NK_DEPLACER: {
						const bool accroche = Accroche(c, ui.accrocheGrille);
						const NkVec2f d(monde.x - ui.gizmoMonde0.x, monde.y - ui.gizmoMonde0.y);
						NkVec2f cible;
						if (!ui.repereLocal) {
							cible = NkVec2f(ui.gizmoCentre0.x + (ui.gizmoTenu == POIGNEE_Y ? 0.f : d.x),
											ui.gizmoCentre0.y + (ui.gizmoTenu == POIGNEE_X ? 0.f : d.y));
							// L'accrochage pose la CIBLE sur la grille (pas le trajet) :
							// l'entite tombe sur un noeud, meme si elle n'en partait pas.
							if (accroche && ui.gizmoTenu != POIGNEE_Y) {
								cible.x = NkEditeurAccrocher(cible.x, ui.pasGrille);
							}
							if (accroche && ui.gizmoTenu != POIGNEE_X) {
								cible.y = NkEditeurAccrocher(cible.y, ui.pasGrille);
							}
						} else {
							// En LOCAL, la grille du monde n'est plus alignee sur les axes :
							// c'est la DISTANCE parcourue le long de chaque axe qui tombe
							// sur un multiple du pas (ce que fait UE5).
							NkVec2f ax, ay;
							AxesMonde(c, ui.gizmoRot0, ax, ay);
							float32 u = ui.gizmoTenu == POIGNEE_Y ? 0.f : Scalaire(d, ax);
							float32 v = ui.gizmoTenu == POIGNEE_X ? 0.f : Scalaire(d, ay);
							if (accroche) {
								u = NkEditeurAccrocher(u, ui.pasGrille);
								v = NkEditeurAccrocher(v, ui.pasGrille);
							}
							cible = Plus(Plus(ui.gizmoCentre0, ax, u), ay, v);
						}
						// (2026-10-01) L'AIMANT apres la grille : un accroche l'emporte.
						NkEditeurDeplacer(m, NkEditeurAimanterDeplacement(c, cible));
						break;
					}
					case NkOutil::NK_TOURNER: {
						const float32 a0 = std::atan2(ui.gizmoMonde0.y - ui.gizmoCentre0.y, ui.gizmoMonde0.x - ui.gizmoCentre0.x);
						const float32 a1 = std::atan2(monde.y - ui.gizmoCentre0.y, monde.x - ui.gizmoCentre0.x);
						float32 total = Replier(a1 - a0);
						if (Accroche(c, ui.accrocheAngle)) {
							total = NkEditeurAccrocher(total * 180.f / PI, ui.pasAngle) * PI / 180.f;
						}
						// L'action recoit la DIFFERENCE avec ce qui est deja applique :
						// elle tourne de delta, elle ne pose pas un angle absolu.
						const float32 pas = total - ui.gizmoAngle;
						if (pas != 0.f && NkEditeurTourner(m, pas)) {
							ui.gizmoAngle = total;
						}
						break;
					}
					case NkOutil::NK_ECHELLE: {
						// Le trajet de la souris projete sur les axes LOCAUX, a l'ecran.
						NkVec2f ax, ay;
						AxesEcran(c, ui.gizmoRot0, ax, ay);
						const NkVec2f trajet(p.x - ui.gizmoEcran0.x, p.y - ui.gizmoEcran0.y);
						const float32 dx = Scalaire(trajet, ax) / GIZMO_AXE;
						const float32 dy = Scalaire(trajet, ay) / GIZMO_AXE;
						NkVec2f f(1.f, 1.f);
						if (ui.gizmoTenu == POIGNEE_X) {
							f.x = 1.f + dx;
						} else if (ui.gizmoTenu == POIGNEE_Y) {
							f.y = 1.f + dy;
						} else {
							const float32 u = 1.f + (dx + dy) * 0.5f;
							f = NkVec2f(u, u);
						}
						if (Accroche(c, ui.accrocheEchelle)) {
							f.x = NkEditeurAccrocher(f.x, ui.pasEchelle);
							f.y = NkEditeurAccrocher(f.y, ui.pasEchelle);
						}
						// Jamais a zero ni negatif : un sprite retourne se fait par le
						// miroir du transform, pas en ecrasant l'echelle.
						f.x = f.x < 0.05f ? 0.05f : f.x;
						f.y = f.y < 0.05f ? 0.05f : f.y;
						const NkVec2f rapport(f.x / ui.gizmoEchelle.x, f.y / ui.gizmoEchelle.y);
						if ((rapport.x != 1.f || rapport.y != 1.f) && NkEditeurMettreAEchelle(m, rapport)) {
							ui.gizmoEchelle = f;
						}
						break;
					}
					default:
						ui.gizmoTenu = POIGNEE_AUCUNE;
						break;
				}
				return true;
			}

			// =================================================================
			// LA SOURIS DU VISEUR
			// =================================================================
			void Souris(NkEditeurCadre &c, const NkRect &aire) {
				NkEditeurModele &m = c.m;
				NkEditeurInterface &ui = c.ui;
				const nkgui::NkGuiInput &in = c.ctx.input;
				const NkVec2f pos(in.mousePos.x, in.mousePos.y);
				// La barre flottante est POSEE sur le viseur : un clic dessus est a elle.
				const bool dedans = NkEditeurDans(aire, in.mousePos) && !NkEditeurDans(ui.barreFlottante, in.mousePos);
				NkVue2D &cam = m.scene.Camera();
				const NkVec2f monde = cam.EcranVersMonde(pos);
				physics::NkParticules2D *p = m.scene.Particules();

				// ── Le gizmo passe AVANT tout : une poignee visee est a lui ──────
				if (GizmoSouris(c, aire)) {
					return;
				}
				// ── Puis la poignee de PORTEE d'une lumiere (2026-09-30) ─────────
				if (NkEditeurGizmoPorteeSouris(c, aire)) {
					return;
				}
				// ── Puis les poignees du COLLISIONNEUR (2026-10-01, NkEditeurPlacer.h) ─
				if (NkEditeurPoigneesCollisionSouris(c, aire)) {
					return;
				}

				// ── Molette : zoom autour du curseur ─────────────────────────────
				// Le point du monde sous le curseur ne bouge pas : c'est lui qu'on
				// regarde, c'est lui qu'on grossit.
				if (dedans && in.wheel != 0.f) {
					ui.cadrageAnime = false; // la main reprend la camera
					const NkVec2f avant = cam.EcranVersMonde(pos);
					const float32 z = cam.Zoom() * (in.wheel > 0.f ? 1.15f : 1.f / 1.15f);
					cam.PoserZoom(z < 4.f ? 4.f : (z > 600.f ? 600.f : z));
					const NkVec2f apres = cam.EcranVersMonde(pos);
					cam.PoserCentre(cam.Centre() + (avant - apres));
				}
				// ── Molette PRESSEE (bouton du milieu) : deplacer la vue ─────────
				// ⚠️ LE SEUL panoramique (2026-09-29) : le bouton droit ouvre le menu,
				//    et le clic gauche dans le vide deselectionne au lieu de tirer la
				//    vue.
				if (dedans && in.mouseClicked[2]) {
					ui.cadrageAnime = false;
					m.panoramique = true;
					m.dernierPointeur = pos;
				}
				if (m.panoramique) {
					if (in.mouseDown[2]) {
						const NkVec2f avant = cam.EcranVersMonde(m.dernierPointeur);
						cam.PoserCentre(cam.Centre() - (monde - avant));
						m.dernierPointeur = pos;
					} else {
						m.panoramique = false;
					}
				}

				// ── Clic droit : le menu contextuel ──────────────────────────────
				// Il CHOISIT d'abord ce qui est sous le curseur (comme UE5) : le menu
				// d'une entite vise celle-la, pas la selection d'avant.
				if (dedans && in.mouseClicked[1]) {
					ui.pointContexte = monde;
					const bool surEntite = NkEditeurCliquerSelection(m, monde);
					NkEditeurOuvrirMenu(c, surEntite ? NkMenuEditeur::NK_CTX_ENTITE : NkMenuEditeur::NK_CTX_VIDE,
										NkRect{pos.x, pos.y, 0.f, 0.f});
					return;
				}

				// ── Appui gauche ─────────────────────────────────────────────────
				if (in.mouseClicked[0] && dedans) {
					m.dernierPointeur = pos;
					ui.precMonde = monde;
					ui.glisserArme = false;
					switch (m.outil) {
						case NkOutil::NK_POSER:
							if (!m.acteurSimple && NkActeurSimInfo(m.acteur).pinceau && m.etat == NkEtatJeu::NK_JEU) {
								// Un fluide en JEU se VERSE tant qu'on maintient.
								m.pinceau = NkOuvrirPinceauSim(m.scene, m.acteur);
								ui.geste = m.pinceau.IsValid();
							} else {
								NkEditeurPoser(m, monde);
							}
							break;
						case NkOutil::NK_EFFACER:
							NkEditeurEffacerSous(m, monde);
							break;
						case NkOutil::NK_SAISIR:
							ui.geste = p != nullptr && p->SaisirDebut(monde, 0.6f);
							if (!ui.geste) {
								NkVec2f centre;
								if (NkEditeurChoisirSous(m, monde, &centre)) {
									m.deplace = true;
									m.decalageSaisie = centre - monde;
								}
							}
							break;
						case NkOutil::NK_COUTEAU:
							ui.geste = true;
							break;
						default: {
							// SELECTION, et les trois gizmos hors de leurs poignees : le clic
							// choisit, le vide deselectionne. Le glisser est ARME, pas encore
							// parti : il ne partira qu'au-dela du seuil.
							NkVec2f centre;
							if (NkEditeurCliquerSelection(m, monde, &centre)) {
								ui.glisserArme = true;
								ui.appuiEcran = pos;
								// Le DECALAGE : sans lui, l'entite sauterait pour se centrer
								// sous le curseur des le premier pixel.
								m.decalageSaisie = centre - monde;
							}
							break;
						}
					}
					return;
				}

				// ── Glissement ───────────────────────────────────────────────────
				// Pas de test d'aire : une saisie commencee se poursuit meme si le
				// curseur sort du viseur.
				if (in.mouseDown[0]) {
					if (ui.geste && m.outil == NkOutil::NK_SAISIR && p != nullptr) {
						p->SaisirVers(monde);
					} else if (ui.geste && m.outil == NkOutil::NK_COUTEAU && p != nullptr) {
						p->Couper(ui.precMonde, monde);
					} else if (ui.geste && m.outil == NkOutil::NK_POSER) {
						if (!NkVerserSim(m.scene, m.pinceau, monde, 3, m.graine)) {
							m.pinceau = NkOuvrirPinceauSim(m.scene, m.acteur);
						}
					} else if (ui.glisserArme && !m.deplace && Distance(pos, ui.appuiEcran) > SEUIL_GLISSER) {
						ui.glisserArme = false;
						if (NkEditeurPeutDeplacer(m)) {
							m.deplace = true;
						} else {
							// DIT, pas tu : un glisser qui ne fait rien sans rien dire
							// passe pour une panne.
							NkEditeurAnnoncer(m, "En jeu, le glisser ne deplace pas : Arreter d'abord (Saisir pour la matiere)");
						}
					}
					if (m.deplace && m.aSelection) {
						// (2026-10-01) L'AIMANT : sommets, aretes, faces des autres objets.
						NkEditeurDeplacer(m, NkEditeurAimanterDeplacement(c, monde + m.decalageSaisie));
					}
					ui.precMonde = monde;
					return;
				}

				// ── Relachement ──────────────────────────────────────────────────
				if (in.mouseReleased[0]) {
					if (p != nullptr) {
						p->SaisirFin();
					}
					ui.geste = false;
					ui.glisserArme = false;
					m.deplace = false;
					m.pinceau = ecs::NkEntityId::Invalid();
				}
			}

			// =================================================================
			// LE REPERE D'AXES, en bas a gauche
			// =================================================================
			/// Fixe a l'ecran, comme le widget d'orientation d'UE5 : il dit ou
			/// sont +X et +Y quel que soit le zoom ou l'endroit regarde.
			void Repere(NkEditeurCadre &c, nkgui::NkGuiDrawList &dl, const NkRect &aire) {
				const NkVec2f o(aire.x + 30.f, aire.y + aire.h - 30.f);
				const NkColor rouge = Role(c, NkRole::AxisX);
				const NkColor vert = Role(c, NkRole::AxisY);
				NkColor fond = c.pal.fond;
				fond.a = 150;
				dl.AddCircleFilled(NkVec2{o.x + 6.f, o.y - 6.f}, 26.f, fond, 32);
				dl.AddLine(NkVec2{o.x, o.y}, NkVec2{o.x + 24.f, o.y}, rouge, 2.f);
				dl.AddTriangleFilled(NkVec2{o.x + 30.f, o.y}, NkVec2{o.x + 23.f, o.y - 4.f}, NkVec2{o.x + 23.f, o.y + 4.f}, rouge);
				dl.AddLine(NkVec2{o.x, o.y}, NkVec2{o.x, o.y - 24.f}, vert, 2.f);
				dl.AddTriangleFilled(NkVec2{o.x, o.y - 30.f}, NkVec2{o.x - 4.f, o.y - 23.f}, NkVec2{o.x + 4.f, o.y - 23.f}, vert);
				renderer::NkTexte(dl, c.petite, o.x + 22.f, o.y + 2.f, "X", rouge);
				renderer::NkTexte(dl, c.petite, o.x + 5.f, o.y - 38.f, "Y", vert);
				dl.AddCircleFilled(NkVec2{o.x, o.y}, 2.5f, c.pal.texte);
			}

			// =================================================================
			// LE CADRAGE ANIME
			// =================================================================
			/// Le zoom qui fait tenir `zone` dans `aire` -- la meme regle que
			/// NkVue2D::Cadrer, bornee comme la molette.
			float32 ZoomPour(const NkRect &aire, const NkVec2f &zone) noexcept {
				if (zone.x <= 0.f || zone.y <= 0.f) {
					return 0.f;
				}
				const float32 zx = aire.w / zone.x;
				const float32 zy = aire.h / zone.y;
				const float32 z = zx < zy ? zx : zy;
				return z < 4.f ? 4.f : (z > 600.f ? 600.f : z);
			}

			void AnimerCadrage(NkEditeurCadre &c, NkVue2D &cam, const NkRect &aire) {
				NkEditeurInterface &ui = c.ui;
				if (ui.cadrageDemande) {
					ui.cadrageDemande = false;
					ui.cadrageCentre0 = cam.Centre();
					ui.cadrageZoom0 = cam.Zoom();
					ui.cadrageZoom1 = ZoomPour(aire, ui.cadrageZone);
					ui.cadrageAnime = ui.cadrageZoom1 > 0.f;
					ui.cadrageT = 0.f;
				}
				if (!ui.cadrageAnime) {
					return;
				}
				ui.cadrageT += ui.dt;
				float32 s = ui.cadrageT / CADRAGE_DUREE;
				s = s > 1.f ? 1.f : s;
				const float32 e = s * s * (3.f - 2.f * s); // lent au depart, lent a l'arrivee
				const NkVec2f c0 = ui.cadrageCentre0;
				const NkVec2f c1 = ui.cadrageCentre1;
				cam.PoserCentre(NkVec2f(c0.x + (c1.x - c0.x) * e, c0.y + (c1.y - c0.y) * e));
				// Le zoom s'interpole en GEOMETRIQUE : de 10 a 100 px/m, la moitie du
				// trajet est 32, pas 55 -- sinon l'image « saute » en fin de course.
				cam.PoserZoom(ui.cadrageZoom0 * std::pow(ui.cadrageZoom1 / ui.cadrageZoom0, e));
				if (s >= 1.f) {
					ui.cadrageAnime = false;
				}
			}

			// =================================================================
			// LA BARRE FLOTTANTE DU VISEUR (2026-09-30)
			// =================================================================
			// Celle d'UE5, dans le coin haut droit : les quatre outils de la
			// selection, le repere du gizmo, et l'accrochage PAR GESTE avec son pas
			// lisible et cliquable. Avant, un seul bouton « Accrochage 0.25 m · 15° »
			// dans la barre de vue allumait tout ou rien, et le pas ne se changeait
			// nulle part.
			enum class NkIconeBarre : uint8 { SELECTION, DEPLACER, TOURNER, ECHELLE, MONDE, LOCAL, GRILLE, ANGLE, PAS_ECHELLE, AIMANT };

			/// Une icone TRACEE (la police embarquee n'a pas ces glyphes).
			void IconeBarre(nkgui::NkGuiDrawList &dl, NkIconeBarre ic, const NkRect &r, const NkColor &col) {
				const float32 cx = r.x + r.w * 0.5f;
				const float32 cy = r.y + r.h * 0.5f;
				auto P = [](float32 x, float32 y) { return NkVec2{x, y}; };
				auto Fleche = [&](float32 x, float32 y, float32 dx, float32 dy) {
					// Une pointe de 3,5 px dans la direction (dx, dy), unitaire.
					const float32 px = -dy;
					const float32 py = dx;
					dl.AddTriangleFilled(P(x + dx * 3.5f, y + dy * 3.5f), P(x + px * 3.f, y + py * 3.f), P(x - px * 3.f, y - py * 3.f), col);
				};
				switch (ic) {
					case NkIconeBarre::SELECTION: {
						const NkVec2 pts[7] = {P(cx - 4.f, cy - 7.f), P(cx - 4.f, cy + 5.f), P(cx - 1.f, cy + 2.f), P(cx + 1.5f, cy + 7.f),
											   P(cx + 3.2f, cy + 6.2f), P(cx + 0.8f, cy + 1.5f), P(cx + 5.f, cy + 1.5f)};
						dl.AddTriangleFilled(pts[0], pts[1], pts[6], col);
						dl.AddPolyline(pts, 7, col, 1.2f, true);
						break;
					}
					case NkIconeBarre::DEPLACER:
						dl.AddLine(P(cx - 6.f, cy), P(cx + 6.f, cy), col, 1.4f);
						dl.AddLine(P(cx, cy - 6.f), P(cx, cy + 6.f), col, 1.4f);
						Fleche(cx + 5.5f, cy, 1.f, 0.f);
						Fleche(cx - 5.5f, cy, -1.f, 0.f);
						Fleche(cx, cy - 5.5f, 0.f, -1.f);
						Fleche(cx, cy + 5.5f, 0.f, 1.f);
						break;
					case NkIconeBarre::TOURNER: {
						NkVec2 arc[16];
						for (int32 k = 0; k < 16; ++k) {
							const float32 a = 0.35f + 4.7f * static_cast<float32>(k) / 15.f;
							arc[k] = P(cx + 6.f * std::cos(a), cy - 6.f * std::sin(a));
						}
						dl.AddPolyline(arc, 16, col, 1.4f, false);
						const float32 af = 0.35f + 4.7f;
						Fleche(arc[15].x, arc[15].y, -std::sin(af), -std::cos(af));
						break;
					}
					case NkIconeBarre::ECHELLE:
						dl.AddRect(NkRect{cx - 6.f, cy - 6.f, 12.f, 12.f}, col, 1.2f);
						dl.AddRectFilled(NkRect{cx - 6.f, cy + 1.f, 5.f, 5.f}, col);
						dl.AddLine(P(cx - 1.f, cy + 1.f), P(cx + 4.f, cy - 4.f), col, 1.3f);
						Fleche(cx + 4.f, cy - 4.f, 0.7071f, -0.7071f);
						break;
					case NkIconeBarre::MONDE:
						// Un globe : le repere du MONDE.
						dl.AddCircle(P(cx, cy), 6.5f, col, 1.2f, 20);
						{
							NkVec2 meridien[16];
							for (int32 k = 0; k < 16; ++k) {
								const float32 a = 6.2831853f * static_cast<float32>(k) / 16.f;
								meridien[k] = P(cx + 2.8f * std::cos(a), cy + 6.5f * std::sin(a));
							}
							dl.AddPolyline(meridien, 16, col, 1.1f, true);
						}
						dl.AddLine(P(cx - 6.5f, cy), P(cx + 6.5f, cy), col, 1.1f);
						break;
					case NkIconeBarre::LOCAL: {
						// Deux axes TOURNES : le repere de l'entite.
						const float32 a = 0.5f;
						const float32 ux = std::cos(a), uy = -std::sin(a);
						const NkVec2 o = P(cx - 4.f, cy + 4.f);
						dl.AddLine(o, P(o.x + ux * 10.f, o.y + uy * 10.f), col, 1.4f);
						dl.AddLine(o, P(o.x + uy * 10.f, o.y - ux * 10.f), col, 1.4f);
						Fleche(o.x + ux * 10.f, o.y + uy * 10.f, ux, uy);
						Fleche(o.x + uy * 10.f, o.y - ux * 10.f, uy, -ux);
						break;
					}
					case NkIconeBarre::AIMANT: {
						// (2026-10-01) Un aimant en fer a cheval, ses deux poles.
						NkVec2 arc[9];
						for (int32 k = 0; k < 9; ++k) {
							const float32 a = 3.14159265f * static_cast<float32>(k) / 8.f;
							arc[k] = P(cx - 5.f * std::cos(a), cy + 1.f - 5.f * std::sin(a));
						}
						dl.AddPolyline(arc, 9, col, 2.6f, false);
						dl.AddLine(P(cx - 5.f, cy + 1.f), P(cx - 5.f, cy + 6.f), col, 2.6f);
						dl.AddLine(P(cx + 5.f, cy + 1.f), P(cx + 5.f, cy + 6.f), col, 2.6f);
						break;
					}
					case NkIconeBarre::GRILLE:
						for (int32 k = 0; k < 4; ++k) {
							const float32 t = -6.f + 4.f * static_cast<float32>(k);
							dl.AddLine(P(cx + t, cy - 6.f), P(cx + t, cy + 6.f), col, 1.f);
							dl.AddLine(P(cx - 6.f, cy + t), P(cx + 6.f, cy + t), col, 1.f);
						}
						break;
					case NkIconeBarre::ANGLE: {
						const NkVec2 o = P(cx - 6.f, cy + 5.f);
						dl.AddLine(o, P(cx + 7.f, cy + 5.f), col, 1.4f);
						dl.AddLine(o, P(cx + 3.f, cy - 6.f), col, 1.4f);
						NkVec2 arc[8];
						for (int32 k = 0; k < 8; ++k) {
							const float32 a = 1.22f * static_cast<float32>(k) / 7.f;
							arc[k] = P(o.x + 7.f * std::cos(a), o.y - 7.f * std::sin(a));
						}
						dl.AddPolyline(arc, 8, col, 1.1f, false);
						break;
					}
					case NkIconeBarre::PAS_ECHELLE:
						dl.AddLine(P(cx - 5.f, cy + 5.f), P(cx + 5.f, cy - 5.f), col, 1.4f);
						Fleche(cx + 5.f, cy - 5.f, 0.7071f, -0.7071f);
						Fleche(cx - 5.f, cy + 5.f, -0.7071f, 0.7071f);
						dl.AddLine(P(cx - 6.5f, cy - 6.5f), P(cx - 1.5f, cy - 6.5f), col, 1.1f);
						dl.AddLine(P(cx - 6.5f, cy - 6.5f), P(cx - 6.5f, cy - 1.5f), col, 1.1f);
						dl.AddLine(P(cx + 6.5f, cy + 6.5f), P(cx + 1.5f, cy + 6.5f), col, 1.1f);
						dl.AddLine(P(cx + 6.5f, cy + 6.5f), P(cx + 6.5f, cy + 1.5f), col, 1.1f);
						break;
				}
			}

			void BarreFlottante(NkEditeurCadre &c, const NkRect &aire) {
				NkEditeurInterface &ui = c.ui;
				auto &dl = c.ctx.dl;
				const nkgui::NkGuiInput &in = c.ctx.input;
				constexpr float32 H = 28.f;	 ///< la barre
				constexpr float32 B = 24.f;	 ///< un bouton d'icone
				constexpr float32 ECART = 2.f;
				constexpr float32 SEP = 9.f; ///< entre deux groupes

				struct NkElement {
						NkIconeBarre icone;
						NkString texte; ///< non vide : un bouton de VALEUR (ouvre les pas)
						int32 action;
						NkMenuEditeur menu;
						bool enfonce;
						bool eteint; ///< la valeur d'un accrochage eteint : attenuee
						bool separateur;
						NkString bulle;
				};
				const NkOutil o = c.m.outil;
				auto Outil = [&](NkIconeBarre ic, NkOutil outil, const char *bulle) {
					return NkElement{ic, NkString(), NK_A_OUTIL + static_cast<int32>(outil), NkMenuEditeur::NK_AUCUN, o == outil, false, false, NkString(bulle)};
				};
				const NkElement elements[12] = {
					Outil(NkIconeBarre::SELECTION, NkOutil::NK_SELECTION, "Sélection (Q)"),
					Outil(NkIconeBarre::DEPLACER, NkOutil::NK_DEPLACER, "Déplacer (W)"),
					Outil(NkIconeBarre::TOURNER, NkOutil::NK_TOURNER, "Tourner (E)"),
					Outil(NkIconeBarre::ECHELLE, NkOutil::NK_ECHELLE, "Échelle (R)"),
					{ui.repereLocal ? NkIconeBarre::LOCAL : NkIconeBarre::MONDE, NkString(), NK_A_REPERE_LOCAL, NkMenuEditeur::NK_AUCUN, false, false, true,
					 NkString(ui.repereLocal ? "Déplacer sur les axes de l'entité (local) — cliquer : monde"
											 : "Déplacer sur les axes du monde — cliquer : local")},
					{NkIconeBarre::GRILLE, NkString(), NK_A_ACCROCHE_GRILLE, NkMenuEditeur::NK_AUCUN, ui.accrocheGrille, false, true,
					 NkString("Accrochage des déplacements (Ctrl l'inverse)")},
					{NkIconeBarre::GRILLE, NkString::Format("%g m", static_cast<double>(ui.pasGrille)), NK_A_AUCUNE, NkMenuEditeur::NK_PAS_GRILLE,
					 ui.menu == NkMenuEditeur::NK_PAS_GRILLE, !ui.accrocheGrille, false, NkString("Pas de la grille")},
					// (2026-10-01) L'AIMANT, a cote de la grille (NkEditeurAimant.cpp).
					{NkIconeBarre::AIMANT, NkString(), NK_A_AIMANT, NkMenuEditeur::NK_AUCUN, ui.aimant, false, false,
					 NkString("Aimant : sommets, arêtes, faces des autres objets (V maintenue l'allume)")},
					{NkIconeBarre::ANGLE, NkString(), NK_A_ACCROCHE_ANGLE, NkMenuEditeur::NK_AUCUN, ui.accrocheAngle, false, false,
					 NkString("Accrochage des rotations (Ctrl l'inverse)")},
					{NkIconeBarre::ANGLE, NkString::Format("%g°", static_cast<double>(ui.pasAngle)), NK_A_AUCUNE, NkMenuEditeur::NK_PAS_ANGLE,
					 ui.menu == NkMenuEditeur::NK_PAS_ANGLE, !ui.accrocheAngle, false, NkString("Pas des angles")},
					{NkIconeBarre::PAS_ECHELLE, NkString(), NK_A_ACCROCHE_ECHELLE, NkMenuEditeur::NK_AUCUN, ui.accrocheEchelle, false, false,
					 NkString("Accrochage des échelles (Ctrl l'inverse)")},
					{NkIconeBarre::PAS_ECHELLE, NkString::Format("x%g", static_cast<double>(ui.pasEchelle)), NK_A_AUCUNE, NkMenuEditeur::NK_PAS_ECHELLE,
					 ui.menu == NkMenuEditeur::NK_PAS_ECHELLE, !ui.accrocheEchelle, false, NkString("Pas de l'échelle")},
				};
				// La largeur d'abord : la barre est calee sur le bord DROIT du viseur.
				float32 largeurs[12];
				float32 total = 4.f;
				for (int32 i = 0; i < 12; ++i) {
					largeurs[i] = elements[i].texte.Empty() ? B : renderer::NkTexteLargeur(c.petite, elements[i].texte.CStr()) + 20.f;
					total += (elements[i].separateur ? SEP : (i > 0 ? ECART : 0.f)) + largeurs[i];
				}
				total += 4.f;
				const NkRect barre{aire.x + aire.w - total - 8.f, aire.y + 8.f, total, H};
				// Trop etroit : la barre se retire plutot que de couvrir la scene.
				if (barre.x < aire.x + 8.f || aire.h < H + 16.f) {
					ui.barreFlottante = NkRect{0.f, 0.f, 0.f, 0.f};
					return;
				}
				ui.barreFlottante = barre;
				NkColor fond = c.pal.entete;
				fond.a = 235;
				dl.AddRectFilled(NkRect{barre.x + 2.f, barre.y + 3.f, barre.w, barre.h}, NkColor{0, 0, 0, 70}, 5.f);
				dl.AddRectFilled(barre, fond, 5.f);
				dl.AddRect(barre, c.pal.bord, 1.f, 5.f);

				float32 x = barre.x + 4.f;
				int32 bulle = -1;
				NkRect rBulle{0.f, 0.f, 0.f, 0.f};
				for (int32 i = 0; i < 12; ++i) {
					const NkElement &e = elements[i];
					if (e.separateur) {
						dl.AddRectFilled(NkRect{x + SEP * 0.5f - 0.5f, barre.y + 6.f, 1.f, H - 12.f}, c.pal.bord);
						x += SEP;
					} else if (i > 0) {
						x += ECART;
					}
					const NkRect r{x, barre.y + (H - B) * 0.5f, largeurs[i], B};
					x += largeurs[i];
					const bool clic = NkEditeurBouton(c, r, "", e.enfonce);
					const NkColor col = e.enfonce ? c.pal.surAccent : (e.eteint ? c.pal.attenue : c.pal.texte);
					if (e.texte.Empty()) {
						IconeBarre(dl, e.icone, r, col);
					} else {
						// La VALEUR, et la pointe « ▾ » qui dit qu'elle se choisit.
						const NkRect rt{r.x, r.y, r.w - 8.f, r.h};
						renderer::NkTexteDansBoite(dl, c.petite, rt, e.texte.CStr(), col);
						const float32 fx = r.x + r.w - 8.f;
						const float32 fy = r.y + r.h * 0.5f;
						dl.AddTriangleFilled(NkVec2{fx - 3.f, fy - 1.5f}, NkVec2{fx + 3.f, fy - 1.5f}, NkVec2{fx, fy + 2.f}, col);
					}
					if (NkEditeurDans(r, in.mousePos)) {
						bulle = i;
						rBulle = r;
					}
					if (clic) {
						if (e.menu != NkMenuEditeur::NK_AUCUN) {
							NkEditeurOuvrirMenu(c, e.menu, r);
						} else {
							NkEditeurExecuter(c, e.action);
						}
					}
				}
				// L'infobulle, SOUS la barre : ce que fait le bouton survole.
				if (bulle >= 0 && ui.menu == NkMenuEditeur::NK_AUCUN) {
					const char *t = elements[bulle].bulle.CStr();
					const float32 tw = renderer::NkTexteLargeur(c.petite, t) + 12.f;
					const float32 th = renderer::NkTexteHauteurLigne(c.petite, 12.f) + 6.f;
					float32 bx = rBulle.x + rBulle.w * 0.5f - tw * 0.5f;
					bx = bx + tw > aire.x + aire.w - 4.f ? aire.x + aire.w - 4.f - tw : bx;
					const NkRect rb{bx, barre.y + barre.h + 4.f, tw, th};
					dl.AddRectFilled(rb, NkColor{16, 18, 24, 235}, 3.f);
					dl.AddRect(rb, c.pal.bord, 1.f, 3.f);
					renderer::NkTexteDansBoite(dl, c.petite, rb, t, c.pal.texte);
				}
			}

		} // namespace

		void NkEditeurDemanderCadrage(NkEditeurCadre &c, bool toutLaScene) {
			NkEditeurModele &m = c.m;
			const bool avait = m.aSelection;
			if (toutLaScene) {
				m.aSelection = false;
			}
			NkVec2f centre(0.f, 0.f);
			NkVec2f taille(0.f, 0.f);
			const bool ok = NkEditeurZoneACadrer(m, centre, taille);
			m.aSelection = avait;
			if (!ok) {
				return;
			}
			c.ui.cadrageDemande = true;
			c.ui.cadrageCentre1 = centre;
			c.ui.cadrageZone = taille;
		}

		void NkEditeurDessinerVue(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			if (ui.vue.w < 8.f || ui.vue.h < 40.f) {
				return;
			}
			BarreVue(c);
			const NkRect aire = ui.viseur;
			if (aire.w < 4.f || aire.h < 4.f) {
				return;
			}
			auto &dl = c.ctx.dl;
			const NkRect appareil = NkAireAppareil(aire, c.m.ProfilCourant(), c.m.appareil.voirCadre);
			// ⚠️ LE VISEUR DE LA CAMERA EST TOUTE L'AIRE (2026-09-29). Il etait
			//    l'aire d'appareil : le monde ne se voyait que dans un rectangle
			//    centre. L'appareil n'est plus qu'une surimpression (NkDessinerViseur).
			NkVue2D &cam = c.m.scene.Camera();
			cam.PoserViseur(aire);
			if (ui.cadrageEnAttente) {
				// A l'ouverture d'une scene : TOUTE la scene, d'un coup (pas de
				// trajet depuis une camera qui ne montrait rien).
				ui.cadrageEnAttente = false;
				ui.cadrageAnime = false;
				const bool avait = c.m.aSelection;
				c.m.aSelection = false;
				NkVec2f centre(0.f, 0.f);
				NkVec2f taille(24.f, 11.f);
				if (!NkEditeurZoneACadrer(c.m, centre, taille)) {
					centre = NkVec2f(0.f, 0.f);
					taille = NkVec2f(24.f, 11.f);
				}
				c.m.aSelection = avait;
				cam.Cadrer(centre, taille);
			}
			AnimerCadrage(c, cam, aire);
			dl.PushClipRect(aire, true);
			c.m.stats = NkDessinerViseur(dl, c.m, aire, appareil);
			DessinerGizmo(c, dl);
			NkEditeurDessinerPoigneesCollision(c, dl); // 2026-10-01
			Repere(c, dl, aire);
			dl.PopClipRect();
			// La barre flottante APRES la scene (elle passe dessus) et AVANT la
			// souris du viseur (un clic sur elle ne choisit rien dessous).
			BarreFlottante(c, aire);
			Souris(c, aire);
		}

	} // namespace editeur
} // namespace nkentseu
