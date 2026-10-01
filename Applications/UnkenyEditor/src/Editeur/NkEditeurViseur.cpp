// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurViseur.cpp
// DESCRIPTION: Le viseur de l'editeur. Il dessine ; il ne modifie rien.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurViseur.h"
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurLumiere.h"
#include "Unkeny/Partie/NkUnkenyPartie.h"
#include "Unkeny/Partie/NkUnkenyZoneSure.h"

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;

		// NkAireAppareil vit avec le cadre, dans NkEditeurCadreAppareil.cpp
		// (2026-10-01) : la place reservee au cadre et son dessin ne doivent pas
		// diverger.

		void NkEditeurPoserEcranDuJeu(NkEditeurModele &m, const nkgui::NkRect &viseur, const nkgui::NkRect &appareil) {
			if (m.profil == 0) {
				renderer::NkLayoutInfo bureau;
				bureau.width = static_cast<uint32>(viseur.w > 1.f ? viseur.w : 1.f);
				bureau.height = static_cast<uint32>(viseur.h > 1.f ? viseur.h : 1.f);
				m.scene.PoserEcran(NkEcranDepuisLayout(bureau, viseur, true));
				return;
			}
			m.scene.PoserEcran(NkEcranDepuisLayout(NkLayoutSimule(m.ProfilCourant()), appareil, true));
		}

		NkCadrageCamera NkEditeurDessinerApercuJeu(nkgui::NkGuiDrawList &dl, NkEditeurModele &m, const nkgui::NkRect &viseur,
												   const nkgui::NkRect &appareil) {
			NkScene &scene = m.scene;
			NkVue2D &cam = scene.Camera();
			const NkRect viseurAvant = cam.Viseur();
			const float32 zoomAvant = cam.Zoom();
			const NkCadrageCamera cad = NkCadrerCamera(m.appareil.regleCamera, viseur.w, viseur.h, zoomAvant, appareil);
			// Les bandes (s'il y en a) sont noires ; l'ecran du jeu a le fond du viseur.
			dl.AddRectFilled(appareil, NkColor(0, 0, 0));
			dl.AddRectFilled(cad.vue, NkColor(20, 23, 31));
			cam.PoserViseur(cad.vue);
			cam.PoserZoom(cad.zoom);
			// L'ecran du jeu se restreint a l'image (les bandes), comme dans le
			// joueur ; il est rendu ensuite.
			const NkEcranDuJeu ecranAvant = scene.Ecran();
			scene.PoserEcran(NkEcranDansVue(ecranAvant, cad.vue));
			// Le HUD se repose pour CETTE camera (sa position monde en depend).
			NkVector<NkPositionAncree> ancrees;
			NkAppliquerAncrages(scene, m.etat == NkEtatJeu::NK_EDITION ? &ancrees : nullptr);
			dl.PushClipRect(cad.vue, true);
			NkDessinerPartie(dl, scene, m.rendu);
			dl.PopClipRect();
			NkRendreAncrages(scene, ancrees);
			scene.PoserEcran(ecranAvant);
			cam.PoserViseur(viseurAvant);
			cam.PoserZoom(zoomAvant);
			return cad;
		}

		NkStatsRendu NkDessinerViseur(nkgui::NkGuiDrawList &dl, NkEditeurModele &m, const nkgui::NkRect &viseur,
									  const nkgui::NkRect &appareil) {
			NkScene &scene = m.scene;
			const NkTheme &th = m.theme;
			const NkProfilAppareil profil = m.ProfilCourant();
			NkStatsRendu stats;
			// ⚠️ LE MONDE REMPLIT TOUT LE VISEUR (Rihen, 2026-09-29). La scene
			// etait dessinee dans le rectangle de l'appareil simule, centre dans le
			// panneau : un cadre dans le cadre, et une part de la vue perdue. Le
			// profil d'appareil reste un REGLAGE ; il se voit en surimpression
			// discrete, plus bas, et seulement s'il n'est pas le bureau lui-meme.
			dl.AddRectFilled(viseur, NkColor(20, 23, 31));
			// Decoupe sur le viseur : la scene ne deborde pas sur les panneaux.
			dl.PushClipRect(viseur, true);
			if (m.voirGrille) {
				NkDessinerGrille(dl, scene.Camera(), 1.f);
				// L'ORIGINE DU MONDE (2026-09-29) : ses deux axes, plus marques que la
				// grille et dans les couleurs du gizmo (X rouge, Y vert), et le point
				// (0, 0). Sans eux, rien ne dit ou est zero -- et une position lue
				// dans les Details ne se retrouve pas a l'oeil.
				const NkVec2f o = scene.Camera().MondeVersEcran(NkVec2f(0.f, 0.f));
				if (o.y >= viseur.y && o.y <= viseur.y + viseur.h) {
					dl.AddLine(NkVec2f(viseur.x, o.y), NkVec2f(viseur.x + viseur.w, o.y), NkColor(210, 75, 75, 170), 1.5f);
				}
				if (o.x >= viseur.x && o.x <= viseur.x + viseur.w) {
					dl.AddLine(NkVec2f(o.x, viseur.y), NkVec2f(o.x, viseur.y + viseur.h), NkColor(85, 190, 95, 170), 1.5f);
				}
				dl.AddCircleFilled(o, 3.5f, NkColor(235, 235, 235, 230));
			}
			// L'ordre : les FORMES (decor, rigides sans texture), puis les sprites,
			// puis la MATIERE par-dessus — elle coule sur tout le reste.
			NkOptionsFormes formes;
			formes.couleurDecor = [](ecs::NkWorld &w, ecs::NkEntityId id, void *) -> uint32 {
				const NkSprite2D *s = w.Get<NkSprite2D>(id);
				return s != nullptr ? s->couleur : 0u; // le sprite (cache) garde la couleur du decor
			};
			// L'OEIL de l'Outliner (NkDrapeauxEditeur::cache), en EDITION : les
			// entites cachees sont ECARTEES le temps du dessin, puis rendues.
			// ⚠️ POURQUOI ECARTER, ET NON « NE PAS DESSINER » : ces dessins sont
			//    ceux d'Unkeny (NkDessinerScene, NkDessinerFormes...), qui ne
			//    connaissent pas l'editeur, et une forme n'a pas de drapeau
			//    « visible ». Le transform est rendu tel quel avant la fin de la
			//    fonction, et aucun pas de simulation ne tourne entre les deux (on
			//    est en EDITION) : la scene ne voit rien passer.
			struct NkEcartee {
					ecs::NkEntityId id;
					NkVec2f position{0.f, 0.f};
					bool aTransform = false;
					bool mouVisible = false;
			};
			// --- L'ECRAN DU JEU (2026-10-01, document 03 §2.5) ------------------
			// Le jeu lit la zone sure dans scene.Ecran() : ici, celle de
			// l'appareil SIMULE (NkLayoutSimule, la structure de NKCanvas), posee
			// sur l'ecran de l'appareil dans le viseur ; le bureau (profil 0) n'a
			// pas de marge, son ecran est le viseur entier. Les entites ancrees a
			// l'ecran (HUD) y sont posees AVANT les ecartees : une entite cachee
			// et ancree reste cachee. En EDITION leurs positions sont RENDUES
			// apres le dessin : la scene editee ne bouge pas.
			NkEditeurPoserEcranDuJeu(m, viseur, appareil);
			NkVector<NkPositionAncree> ancrees;
			NkAppliquerAncrages(scene, m.etat == NkEtatJeu::NK_EDITION ? &ancrees : nullptr);
			NkVector<NkEcartee> ecartees;
			if (m.etat == NkEtatJeu::NK_EDITION) {
				// Elle, OU un ancetre : un parent cache cache sa descendance.
				scene.Monde().Query<NkTransform2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &) {
					if (!NkEditeurCacheDansLaVue(m, id)) {
						return;
					}
					NkEcartee e;
					e.id = id;
					if (NkTransform2D *t = scene.Monde().Get<NkTransform2D>(id)) {
						e.position = t->position;
						e.aTransform = true;
						t->position = NkVec2f(1.0e6f, 1.0e6f); // hors de toute vue : le hors-champ l'ecarte
					}
					if (NkCorpsMou2D *mou = scene.Monde().Get<NkCorpsMou2D>(id)) {
						e.mouVisible = mou->visible;
						mou->visible = false; // la matiere ne suit pas le transform
					}
					ecartees.PushBack(e);
				});
			}
			// L'IMAGE DU JEU -- formes, sprites, matiere -- par la fonction que
			// dessine aussi le joueur autonome (Unkeny/Partie) : ce que montre le
			// viseur est ce que montrera le jeu construit.
			// Les effets (fumee sous la lumiere, feu par-dessus) et la carte de
			// lumiere en font PARTIE depuis le 2026-09-30 : un jeu construit a
			// son feu et sa nuit. L'editeur n'en retient que les chiffres (barre
			// d'etat, onglet Monde) ; les surcouches viennent apres, non eclairees.
			NkStatsEclairage2D eclairage;
			int32 particules = 0;
			stats = NkDessinerPartie(dl, scene, m.rendu, &eclairage, &particules);
			NkEditeurRetenirChiffresLumiere(eclairage, particules);
			if (m.voirCollisionneurs) {
				NkDessinerCollisionneurs(dl, scene, 0x00E07AC0u);
			}
			for (uint32 i = 0; i < ecartees.Size(); ++i) {
				const NkEcartee &e = ecartees[i];
				if (e.aTransform) {
					if (NkTransform2D *t = scene.Monde().Get<NkTransform2D>(e.id)) {
						t->position = e.position;
					}
				}
				if (NkCorpsMou2D *mou = scene.Monde().Get<NkCorpsMou2D>(e.id)) {
					mou->visible = e.mouVisible;
				}
			}
			// Les entites SANS VISUEL (un transform, un nom, une source sonore...) :
			// un losange, en EDITION, comme les icones d'acteurs d'UE5. Sans lui,
			// une entite vide posee dans la scene ne se voit ni ne se clique
			// (NkEditeurPrendreSous vise ce meme losange).
			if (m.etat == NkEtatJeu::NK_EDITION) {
				const NkColor fondMarqueur(150, 160, 185, 90);
				const NkColor bordMarqueur(200, 208, 225, 230);
				scene.Monde().Query<NkTransform2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t) {
					if (!NkEditeurSansVisuel(m, id) || NkEditeurCacheDansLaVue(m, id) || !scene.EstActive(id)) {
						return;
					}
					const NkVec2f e = scene.Camera().MondeVersEcran(t.position);
					const float32 r = NK_MARQUEUR_RAYON_PX;
					const NkVec2f pts[4] = {NkVec2f(e.x, e.y - r), NkVec2f(e.x + r, e.y), NkVec2f(e.x, e.y + r), NkVec2f(e.x - r, e.y)};
					dl.AddTriangleFilled(pts[0], pts[1], pts[2], fondMarqueur);
					dl.AddTriangleFilled(pts[0], pts[2], pts[3], fondMarqueur);
					dl.AddPolyline(pts, 4, bordMarqueur, 1.5f, true);
					dl.AddCircleFilled(e, 1.5f, bordMarqueur);
				});
			}
			// La selection : un cadre d'accent autour de sa boite, et son nom.
			// Pas pour une entite cachee : l'oeil ferme ne laisse RIEN dans la vue.
			NkVec2f mn, mx;
			const bool selectionCachee = m.aSelection && NkEditeurCacheDansLaVue(m, m.selection);
			if (!selectionCachee && NkEditeurBoiteSelection(m, mn, mx)) {
				const NkVec2f hg = scene.Camera().MondeVersEcran(NkVec2f(mn.x - 0.05f, mx.y + 0.05f));
				const NkVec2f bd = scene.Camera().MondeVersEcran(NkVec2f(mx.x + 0.05f, mn.y - 0.05f));
				dl.AddRect(NkRect{hg.x, hg.y, bd.x - hg.x, bd.y - hg.y}, th.or_, 2.f, 3.f);
				NkVec2f c;
				if (NkEditeurCentreSelection(m, c)) {
					// Une croix au centre : elle dit ou est l'ORIGINE de l'entite.
					const NkVec2f e = scene.Camera().MondeVersEcran(c);
					dl.AddLine(NkVec2f(e.x - 6.f, e.y), NkVec2f(e.x + 6.f, e.y), th.or_, 1.5f);
					dl.AddLine(NkVec2f(e.x, e.y - 6.f), NkVec2f(e.x, e.y + 6.f), th.or_, 1.5f);
				}
			}
			NkEditeurDessinerIconesLumiere(dl, m);
			dl.PopClipRect();
			NkRendreAncrages(scene, ancrees);

			// Le bord du viseur dit l'ETAT — VERT en jeu, AMBRE en pause : on sait
			// d'un regard si ce qu'on voit est la scene editee ou un instant de
			// simulation. En edition, rien : le viseur n'a pas a s'encadrer.
			if (m.etat != NkEtatJeu::NK_EDITION) {
				const NkColor bordEtat = m.etat == NkEtatJeu::NK_JEU ? th.succes : th.or_;
				dl.AddRect(viseur, bordEtat, 2.f);
			}

			// --- L'APPAREIL SIMULE, EN SURIMPRESSION ----------------------
			// Le bureau n'a ni encoche ni indicateur de geste, et son ecran EST
			// la vue : il n'y a rien a surimprimer.
			if (m.profil == 0) {
				return stats;
			}
			// (2026-10-01) L'APERCU DE LA CAMERA DU JEU (document 03, §2.6) : dans
			// l'ecran de l'appareil, ce que le joueur montrerait, selon la regle
			// du projet -- la reference est CE viseur et CE zoom, ceux que
			// Construire cuirait maintenant.
			if (m.appareil.apercuJeu) {
				NkEditeurDessinerApercuJeu(dl, m, viseur, appareil);
			}
			// Le CADRE en vecteurs, la zone sure, la decoupe : selon les
			// interrupteurs du menu Appareil (document 03, §2.4).
			NkDessinerAppareil(dl, profil, appareil, m.appareil, viseur);
			return stats;
		}

	} // namespace editeur
} // namespace nkentseu
