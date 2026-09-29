//
// NkEditeurVue.cpp
// =============================================================================
// Description :
//   La colonne centrale : la BARRE DE VUE (Cadrer, Grille, Collisionneurs ;
//   l'appareil et le zoom a droite) et le VISEUR 2D, avec sa souris.
//
// Caracteristiques :
//   - Le dessin de la scene est celui de NkEditeurViseur.cpp (inchange) : ce
//     fichier ne fait que lui donner une aire et y router la souris.
//   - La souris reprend tel quel le comportement de l'ancien panneau viseur :
//     molette = zoom autour du curseur, bouton droit ou milieu = deplacer la
//     vue, bouton gauche = l'outil courant (selection, pose, gomme, saisie,
//     couteau). Chaque geste appelle une action de NkEditeurActions.h.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurViseur.h"

#include "NKCanvas/App/NkCanvasTexte.h"

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;

		namespace {

			/// La barre de vue : un groupe a gauche, les reperes a droite (UI_SPEC
			/// §2.2, regle 3). Ce qui s'y lit est VRAI en continu : le zoom et
			/// l'appareil sont relus a chaque trame.
			void BarreVue(NkEditeurCadre &c) {
				const NkRect &b = c.ui.barreVue;
				auto &dl = c.ctx.dl;
				dl.AddRectFilled(b, c.pal.entete);
				dl.AddRectFilled(NkRect{b.x, b.y + b.h - 1.f, b.w, 1.f}, c.pal.bord);
				float32 x = b.x + 6.f;
				struct NkBouton {
						const char *texte;
						int32 action;
						bool enfonce;
				};
				const NkBouton boutons[3] = {
					{"Cadrer", NK_A_CADRER, false},
					{"Grille", NK_A_GRILLE, c.m.voirGrille},
					{"Collisionneurs", NK_A_COLLISIONNEURS, c.m.voirCollisionneurs},
				};
				for (int32 i = 0; i < 3; ++i) {
					const float32 w = renderer::NkTexteLargeur(c.petite, boutons[i].texte) + 16.f;
					const NkRect r{x, b.y + 4.f, w, b.h - 8.f};
					const bool clic = NkEditeurBouton(c, r, "", boutons[i].enfonce);
					renderer::NkTexteDansBoite(dl, c.petite, r, boutons[i].texte,
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

			// -----------------------------------------------------------------
			// La souris du viseur, reprise de l'ancien NkPanneauViseur::Souris.
			// -----------------------------------------------------------------
			void Souris(NkEditeurCadre &c, const NkRect &aire) {
				NkEditeurModele &m = c.m;
				NkEditeurInterface &ui = c.ui;
				const nkgui::NkGuiInput &in = c.ctx.input;
				const NkVec2f pos(in.mousePos.x, in.mousePos.y);
				const bool dedans = NkEditeurDans(aire, in.mousePos);
				NkVue2D &cam = m.scene.Camera();
				const NkVec2f monde = cam.EcranVersMonde(pos);
				physics::NkParticules2D *p = m.scene.Particules();

				// ── Molette : zoom autour du curseur ─────────────────────────────
				if (dedans && in.wheel != 0.f) {
					const NkVec2f avant = cam.EcranVersMonde(pos);
					const float32 z = cam.Zoom() * (in.wheel > 0.f ? 1.15f : 1.f / 1.15f);
					cam.PoserZoom(z < 4.f ? 4.f : (z > 600.f ? 600.f : z));
					const NkVec2f apres = cam.EcranVersMonde(pos);
					cam.PoserCentre(cam.Centre() + (avant - apres));
				}
				// ── Bouton du milieu ou droit : deplacer la vue, quel que soit l'outil
				if (dedans && (in.mouseClicked[1] || in.mouseClicked[2])) {
					m.panoramique = true;
					m.dernierPointeur = pos;
				}
				if (m.panoramique && (in.mouseDown[1] || in.mouseDown[2] || in.mouseDown[0])) {
					const NkVec2f avant = cam.EcranVersMonde(m.dernierPointeur);
					cam.PoserCentre(cam.Centre() - (monde - avant));
					m.dernierPointeur = pos;
				}

				// ── Appui ────────────────────────────────────────────────────────
				if (in.mouseClicked[0] && dedans) {
					m.dernierPointeur = pos;
					ui.precMonde = monde;
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
						default: { // NK_SELECTION
							NkVec2f centre;
							if (NkEditeurChoisirSous(m, monde, &centre)) {
								m.deplace = true;
								// On retient le DECALAGE : sans lui, l'entite saute pour
								// se centrer sous le curseur des le premier pixel.
								m.decalageSaisie = centre - monde;
							} else {
								m.panoramique = true; // clic dans le vide = on deplace la vue
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
					} else if (m.deplace && m.aSelection) {
						NkEditeurDeplacer(m, monde + m.decalageSaisie);
					}
					ui.precMonde = monde;
					return;
				}

				// ── Relachement ──────────────────────────────────────────────────
				if (in.mouseReleased[0] || in.mouseReleased[1] || in.mouseReleased[2]) {
					if (p != nullptr) {
						p->SaisirFin();
					}
					ui.geste = false;
					m.deplace = false;
					m.panoramique = false;
					m.pinceau = ecs::NkEntityId::Invalid();
				}
			}

		} // namespace

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
				ui.cadrageEnAttente = false;
				cam.Cadrer(NkVec2f(0.f, 0.f), NkVec2f(24.f, 11.f));
			}
			dl.PushClipRect(aire, true);
			c.m.stats = NkDessinerViseur(dl, c.m, aire, appareil);
			dl.PopClipRect();
			Souris(c, aire);
		}

	} // namespace editeur
} // namespace nkentseu
