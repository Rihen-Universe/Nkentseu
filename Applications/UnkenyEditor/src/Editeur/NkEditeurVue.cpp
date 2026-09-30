//
// NkEditeurVue.cpp
// =============================================================================
// Description :
//   La colonne centrale : la BARRE DE VUE (Cadrer, Grille, Collisionneurs,
//   Accrochage ; l'appareil et le zoom a droite) et le VISEUR 2D, avec sa
//   souris, ses GIZMOS, son repere d'axes et son cadrage anime.
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

			/// L'accrochage de CE geste : le reglage, inverse tant que Ctrl est tenu.
			bool Accroche(NkEditeurCadre &c) noexcept {
				return c.ui.accrochage != c.ctx.input.ctrlDown;
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
				const NkBouton boutons[4] = {
					{NkString("Cadrer"), NK_A_CADRER_SELECTION, false},
					{NkString("Grille"), NK_A_GRILLE, c.m.voirGrille},
					{NkString("Collisionneurs"), NK_A_COLLISIONNEURS, c.m.voirCollisionneurs},
					// Le pas se LIT sur le bouton : c'est la ou UE5 le montre, et un
					// accrochage dont on ne voit pas la valeur surprend.
					{NkString::Format("Accrochage %.2f m · %.0f°", static_cast<double>(c.ui.pasGrille),
									  static_cast<double>(c.ui.pasAngle)),
					 NK_A_ACCROCHAGE, c.ui.accrochage},
				};
				for (int32 i = 0; i < 4; ++i) {
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
					   !NkEditeurEstVerrouille(c.m, c.m.selection) && !NkEditeurCacheDansLaVue(c.m, c.m.selection);
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

			/// Le carre du plan XY (deplacer) ou de l'echelle uniforme.
			NkRect CarreGizmo(const NkVec2f &o, NkOutil outil) noexcept {
				if (outil == NkOutil::NK_ECHELLE) {
					return NkRect{o.x - 7.f, o.y - 7.f, 14.f, 14.f};
				}
				return NkRect{o.x + 12.f, o.y - 28.f, 16.f, 16.f};
			}

			/// La poignee sous `p`. Le carre d'abord : il est POSE sur les fleches,
			/// et le viser doit l'emporter.
			int32 PoigneeSous(const NkVec2f &o, NkOutil outil, const NkVec2f &p) noexcept {
				if (outil == NkOutil::NK_TOURNER) {
					return math::NkAbs(Distance(p, o) - GIZMO_ANNEAU) < GIZMO_PRISE ? POIGNEE_ANNEAU : POIGNEE_AUCUNE;
				}
				if (Dans(CarreGizmo(o, outil), p)) {
					return POIGNEE_XY;
				}
				// L'axe Y de l'ECRAN monte : le +Y du monde est vers le haut.
				if (DistanceSegment(p, NkVec2f(o.x + 10.f, o.y), NkVec2f(o.x + GIZMO_AXE + 6.f, o.y)) < GIZMO_PRISE) {
					return POIGNEE_X;
				}
				if (DistanceSegment(p, NkVec2f(o.x, o.y - 10.f), NkVec2f(o.x, o.y - GIZMO_AXE - 6.f)) < GIZMO_PRISE) {
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
				const NkVec2f finX(o.x + GIZMO_AXE, o.y);
				const NkVec2f finY(o.x, o.y - GIZMO_AXE);
				dl.AddLine(NkVec2{o.x, o.y}, NkVec2{finX.x, finX.y}, cx, 2.5f);
				dl.AddLine(NkVec2{o.x, o.y}, NkVec2{finY.x, finY.y}, cy, 2.5f);
				if (outil == NkOutil::NK_DEPLACER) {
					// Des FLECHES : on tire dans leur direction.
					dl.AddTriangleFilled(NkVec2{finX.x + 12.f, finX.y}, NkVec2{finX.x, finX.y - 6.f}, NkVec2{finX.x, finX.y + 6.f}, cx);
					dl.AddTriangleFilled(NkVec2{finY.x, finY.y - 12.f}, NkVec2{finY.x - 6.f, finY.y}, NkVec2{finY.x + 6.f, finY.y}, cy);
				} else {
					// Des CUBES au bout : on etire.
					dl.AddRectFilled(NkRect{finX.x - 5.f, finX.y - 5.f, 10.f, 10.f}, cx);
					dl.AddRectFilled(NkRect{finY.x - 5.f, finY.y - 5.f, 10.f, 10.f}, cy);
				}
				const NkRect carre = CarreGizmo(o, outil);
				const bool carreVif = vive == POIGNEE_XY;
				NkColor fond = carreVif ? ambre : Role(c, NkRole::AxisZ);
				fond.a = carreVif ? 200 : 110;
				dl.AddRectFilled(carre, fond);
				dl.AddRect(carre, carreVif ? ambre : c.pal.texte, 1.f);
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
					if (!GizmoVisible(c) || !Dans(aire, p) || !CentreGizmo(c, o)) {
						return false;
					}
					ui.gizmoSurvol = PoigneeSous(o, m.outil, p);
					if (ui.gizmoSurvol == POIGNEE_AUCUNE || !in.mouseClicked[0]) {
						return false;
					}
					ui.gizmoTenu = ui.gizmoSurvol;
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
				const bool accroche = Accroche(c);
				switch (m.outil) {
					case NkOutil::NK_DEPLACER: {
						NkVec2f d(monde.x - ui.gizmoMonde0.x, monde.y - ui.gizmoMonde0.y);
						if (ui.gizmoTenu == POIGNEE_X) {
							d.y = 0.f;
						} else if (ui.gizmoTenu == POIGNEE_Y) {
							d.x = 0.f;
						}
						NkVec2f cible(ui.gizmoCentre0.x + d.x, ui.gizmoCentre0.y + d.y);
						// L'accrochage pose la CIBLE sur la grille (pas le trajet) : l'entite
						// tombe sur un noeud, meme si elle n'en partait pas.
						if (accroche && ui.gizmoTenu != POIGNEE_Y) {
							cible.x = NkEditeurAccrocher(cible.x, ui.pasGrille);
						}
						if (accroche && ui.gizmoTenu != POIGNEE_X) {
							cible.y = NkEditeurAccrocher(cible.y, ui.pasGrille);
						}
						NkEditeurDeplacer(m, cible);
						break;
					}
					case NkOutil::NK_TOURNER: {
						const float32 a0 = std::atan2(ui.gizmoMonde0.y - ui.gizmoCentre0.y, ui.gizmoMonde0.x - ui.gizmoCentre0.x);
						const float32 a1 = std::atan2(monde.y - ui.gizmoCentre0.y, monde.x - ui.gizmoCentre0.x);
						float32 total = Replier(a1 - a0);
						if (accroche) {
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
						const float32 dx = (p.x - ui.gizmoEcran0.x) / GIZMO_AXE;
						const float32 dy = -(p.y - ui.gizmoEcran0.y) / GIZMO_AXE; // l'ecran descend, le monde monte
						NkVec2f f(1.f, 1.f);
						if (ui.gizmoTenu == POIGNEE_X) {
							f.x = 1.f + dx;
						} else if (ui.gizmoTenu == POIGNEE_Y) {
							f.y = 1.f + dy;
						} else {
							const float32 u = 1.f + (dx + dy) * 0.5f;
							f = NkVec2f(u, u);
						}
						if (accroche) {
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
				const bool dedans = NkEditeurDans(aire, in.mousePos);
				NkVue2D &cam = m.scene.Camera();
				const NkVec2f monde = cam.EcranVersMonde(pos);
				physics::NkParticules2D *p = m.scene.Particules();

				// ── Le gizmo passe AVANT tout : une poignee visee est a lui ──────
				if (GizmoSouris(c, aire)) {
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
						NkEditeurDeplacer(m, monde + m.decalageSaisie);
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
			const NkRect appareil = NkAireAppareil(aire, c.m.ProfilCourant());
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
			Repere(c, dl, aire);
			dl.PopClipRect();
			Souris(c, aire);
		}

	} // namespace editeur
} // namespace nkentseu
