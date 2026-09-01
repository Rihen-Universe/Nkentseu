// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurPanneaux.cpp
// DESCRIPTION: Les panneaux de l'editeur, en NkEditorPanel (NKEditorKit).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurPanneaux.h"

#include "Editeur/NkEditeurViseur.h"
#include "NKContainers/String/NkString.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

namespace nkentseu {
	namespace editeur {

		using nkgui::NkRect;

		namespace {

			/// Le libelle d'une entite : son etiquette, ou son indice a defaut.
			NkString NomDe(NkScene &scene, ecs::NkEntityId id) {
				const NkEtiquette *e = scene.Monde().Get<NkEtiquette>(id);
				if (e != nullptr && e->nom[0] != '\0') {
					return NkString(e->nom);
				}
				return NkString::Format("Entite %u", static_cast<uint32>(id.index));
			}

			/// Une ligne « cle : valeur » en lecture seule.
			void Ligne(NkEditorFrameContext &ec, const char *cle, const NkString &valeur) {
				ec.Text(NkString::Format("%s : %s", cle, valeur.Data()).Data());
			}

		} // namespace

		// =====================================================================
		// LE VISEUR
		// =====================================================================
		void NkPanneauViseur::OnUI(NkEditorFrameContext &ec) {
			auto &ctx = ec.Ui();

			// L'aire du viseur = tout ce qui reste sous le curseur du panneau.
			// ⚠️ On la redemande A CHAQUE TRAME : avec l'ancrage, un panneau
			// change de taille quand on deplace une cloison, et une aire retenue
			// d'une trame sur l'autre ferait diverger le dessin de la souris.
			const NkRect aire = ctx.NextItemRect(-1.f, ctx.AvailHeight());
			if (aire.w < 4.f || aire.h < 4.f) {
				return; // panneau reduit a rien : il n'y a rien a dessiner
			}

			const NkProfilAppareil profil = mM.ProfilCourant();
			const NkRect appareil = NkAireAppareil(aire, profil);

			// ⚠️ Le viseur de la CAMERA est l'aire d'appareil, PAS le panneau.
			// Sans cela, le cadrage mentirait sur ce que verrait un telephone :
			// on jugerait une mise en page mobile sur un rectangle de bureau.
			mM.scene.Camera().PoserViseur(appareil);

			// ── LE PAS DE PHYSIQUE VIT ICI, ET IL FAUT SAVOIR POURQUOI ──────
			// `NkEditorShell::Run()` est une boucle BLOQUANTE : le kit n offre
			// aucun crochet « par trame » a l application. Le seul endroit ou du
			// temps s ecoule et ou l on recoit un `dt` est le dessin d un
			// panneau. C est donc le viseur qui avance la simulation.
			//
			// ⚠️ CONSEQUENCE A DIRE PLUTOT QU A DECOUVRIR : fermer le viseur
			// MET LA SIMULATION EN PAUSE. C est defendable -- sans viseur il n y
			// a rien a regarder -- mais ce n est PAS evident, et quelqu un
			// chercherait sinon pourquoi ses caisses ne tombent plus.
			//
			// ⚠️ Et la physique n avance QUE si on l a demandee : un editeur qui
			// simule en permanence ne permet pas de POSER quoi que ce soit,
			// l objet tombe avant qu on ait lache le bouton.
			if (mM.simuler && ec.dt > 0.f) {
				mM.scene.Pas(ec.dt);
			}

			mM.stats = NkDessinerViseur(ctx.DL(), mM.scene, aire, appareil, mM.theme, profil, mM.voirGrille,
										mM.voirCollisionneurs, mM.SelectionOuNul());

			Souris(ec, aire);
		}

		// ---------------------------------------------------------------------
		// La souris.
		//
		// ⚠️ `ctx.InputHits` PLUTOT QU'UN TEST DE RECTANGLE. Il verifie en plus
		//    que le point n'est pas recouvert par une couche superieure. Ma
		//    premiere version testait « le point est-il dans le rectangle » : un
		//    clic tombe sur un panneau flottant AU-DESSUS du viseur atteignait
		//    quand meme la scene. Le defaut ne se voit que le jour ou une modale
		//    s'ouvre par-dessus -- et il ressemble alors a un bug de la modale.
		// ---------------------------------------------------------------------
		void NkPanneauViseur::Souris(NkEditorFrameContext &ec, const NkRect &aire) {
			auto &ctx = ec.Ui();
			const auto &in = ctx.input;
			const NkVec2f pos(in.mousePos.x, in.mousePos.y);
			const bool dedans = ctx.InputHits(aire);
			const NkVec2f monde = mM.scene.Camera().EcranVersMonde(pos);

			// ── Appui ────────────────────────────────────────────────────────
			if (in.mouseClicked[0] && dedans) {
				mM.dernierPointeur = pos;

				if (mM.outil == NkOutil::NK_POSER) {
					PoserEntite(monde);
				} else if (mM.outil == NkOutil::NK_EFFACER) {
					ecs::NkEntityId trouve;
					NkVec2f centre;
					if (NkEntiteSous(mM.scene, monde, trouve, centre)) {
						mM.scene.Detruire(trouve);
						mM.aSelection = false;
					}
				} else { // NK_SELECTION
					ecs::NkEntityId trouve;
					NkVec2f centre;
					if (NkEntiteSous(mM.scene, monde, trouve, centre)) {
						mM.selection = trouve;
						mM.aSelection = true;
						mM.deplace = true;
						// On retient le DECALAGE : sans lui, l'entite saute pour
						// se centrer sous le curseur des le premier pixel.
						mM.decalageSaisie = NkVec2f(centre.x - monde.x, centre.y - monde.y);
					} else {
						mM.aSelection = false;
						mM.panoramique = true; // clic dans le vide = on deplace la vue
					}
				}
				return;
			}

			// ── Glissement ───────────────────────────────────────────────────
			// Pas de test d'occultation ici, et c'est VOULU : une fois la saisie
			// commencee, elle se poursuit meme si le curseur sort du panneau.
			// L'interrompre au bord ferait « lacher » l'objet sans qu'on ait
			// relache le bouton.
			if (in.mouseDown[0]) {
				if (mM.deplace && mM.aSelection) {
					// ⚠️ On TELEPORTE : poser directement le transform laisserait
					// le solveur a l'ancienne position, et l'entite reviendrait
					// d'un coup au pas suivant.
					mM.scene.TeleporterEntite(mM.selection,
											  NkVec2f(monde.x + mM.decalageSaisie.x, monde.y + mM.decalageSaisie.y));
					return;
				}
				if (mM.panoramique) {
					// Le panoramique se calcule en MONDE : en pixels, il irait
					// plus ou moins vite selon le zoom.
					const NkVec2f avant = mM.scene.Camera().EcranVersMonde(mM.dernierPointeur);
					const NkVec2f c = mM.scene.Camera().Centre();
					mM.scene.Camera().PoserCentre(NkVec2f(c.x - (monde.x - avant.x), c.y - (monde.y - avant.y)));
					mM.dernierPointeur = pos;
					return;
				}
			}

			// ── Relachement ──────────────────────────────────────────────────
			if (in.mouseReleased[0]) {
				mM.deplace = false;
				mM.panoramique = false;
			}
		}

		void NkPanneauViseur::CadrerSurTout() noexcept {
			mM.scene.Camera().Cadrer(NkVec2f(0.f, -1.f), NkVec2f(22.f, 14.f));
		}

		void NkPanneauViseur::PoserEntite(const NkVec2f &monde) {
			mM.graine = mM.graine * 1664525u + 1013904223u;
			const uint32 teinte = 0x40404000u | ((mM.graine >> 8) & 0x00BFBFBFu) | 0xFFu;

			const ecs::NkEntityId e = mM.scene.Creer("Entite", monde);

			NkSprite2D s;
			s.taille = NkVec2f(0.8f, 0.8f);
			s.couleur = teinte;
			mM.scene.Monde().Add<NkSprite2D>(e, s);

			NkCollisionneur2D c;
			c.forme = NkForme2D::NK_BOITE;
			c.demiTaille = NkVec2f(0.4f, 0.4f);
			mM.scene.Monde().Add<NkCollisionneur2D>(e, c);

			NkCorps2D b;
			b.type = NkTypeCorps::NK_DYNAMIQUE;
			mM.scene.AjouterCorps(e, b);

			mM.selection = e;
			mM.aSelection = true;
		}

		// =====================================================================
		// LA HIERARCHIE
		// =====================================================================
		void NkPanneauHierarchie::OnUI(NkEditorFrameContext &ec) {
			auto &ctx = ec.Ui();

			int32 n = 0;
			mM.scene.Monde().Query<NkTransform2D>().ForEach([&](ecs::NkEntityId, NkTransform2D &) { ++n; });
			ec.Text(NkString::Format("%d entite(s)", n).Data());
			ec.Separator();

			mM.scene.Monde().Query<NkTransform2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &) {
				// ⚠️ `NkEntityId` porte SON operator== : il compare l'index ET la
				// generation. Comparer le seul index confondrait une entite
				// detruite avec celle qui a repris sa place -- c'est exactement
				// ce que la generation existe pour empecher.
				const bool choisie = mM.aSelection && mM.selection == id;
				if (nkgui::Selectable(ctx, NomDe(mM.scene, id).Data(), choisie)) {
					mM.selection = id;
					mM.aSelection = true;
				}
			});
		}

		// =====================================================================
		// L'INSPECTEUR
		//
		// ⚠️ IL EST MODIFIABLE, ET IL NE L'ETAIT PAS. Ma premiere version
		//    AFFICHAIT les valeurs sans permettre de les changer -- un inspecteur
		//    en lecture seule n'inspecte pas, il rapporte. Les curseurs et les
		//    cases viennent du contexte du kit : rien a dessiner a la main.
		//
		// ⚠️ ET CE N'EST PAS `NkEditorInspector`, DELIBEREMENT. Celui du kit est
		//    pilote par NKReflection et n'affiche que des classes REFLECHIES ;
		//    les composants d'Unkeny sont des structures nues. Le rendre
		//    generique demanderait de reflechir les composants du moteur --
		//    chantier reel, et qui n'est pas celui-ci.
		// =====================================================================
		void NkPanneauInspecteur::OnUI(NkEditorFrameContext &ec) {
			if (!mM.aSelection) {
				ec.Text("Aucune selection");
				return;
			}
			ecs::NkEntityId id = mM.selection;
			auto &monde = mM.scene.Monde();

			ec.Text(NomDe(mM.scene, id).Data());
			ec.Separator();

			if (NkTransform2D *t = monde.Get<NkTransform2D>(id)) {
				ec.Text("Transform");
				// ⚠️ La position passe par TeleporterEntite, jamais par une
				// ecriture directe : le solveur garde sinon l'ancienne position
				// et l'objet revient d'un coup au pas suivant.
				float32 x = t->position.x;
				float32 y = t->position.y;
				const bool bougeX = ec.SliderFloat("x", x, -20.f, 20.f);
				const bool bougeY = ec.SliderFloat("y", y, -20.f, 20.f);
				if (bougeX || bougeY) {
					mM.scene.TeleporterEntite(id, NkVec2f(x, y));
				}
				float32 rot = t->rotation * 57.2957795f;
				if (ec.SliderFloat("rotation", rot, -180.f, 180.f)) {
					t->rotation = rot / 57.2957795f;
				}
				ec.Separator();
			}

			if (NkSprite2D *s = monde.Get<NkSprite2D>(id)) {
				ec.Text("Sprite");
				ec.SliderFloat("largeur", s->taille.x, 0.05f, 20.f);
				ec.SliderFloat("hauteur", s->taille.y, 0.05f, 20.f);
				ec.Checkbox("visible", s->visible);
				Ligne(ec, "couche", NkString::Format("%d", s->couche));
				ec.Separator();
			}

			if (NkCollisionneur2D *c = monde.Get<NkCollisionneur2D>(id)) {
				ec.Text("Collisionneur");
				const char *f = (c->forme == NkForme2D::NK_CERCLE)
									? "cercle"
									: ((c->forme == NkForme2D::NK_CAPSULE) ? "capsule" : "boite");
				Ligne(ec, "forme", NkString(f));
				ec.SliderFloat("demi-largeur", c->demiTaille.x, 0.02f, 10.f);
				ec.SliderFloat("demi-hauteur", c->demiTaille.y, 0.02f, 10.f);
				ec.Checkbox("declencheur", c->declencheur);
				ec.Separator();
			}

			if (const NkCorps2D *b = monde.Get<NkCorps2D>(id)) {
				ec.Text("Corps");
				const char *ty = (b->type == NkTypeCorps::NK_STATIQUE)
									 ? "statique"
									 : ((b->type == NkTypeCorps::NK_CINEMATIQUE) ? "cinematique" : "dynamique");
				Ligne(ec, "type", NkString(ty));
			}
		}

		// =====================================================================
		// OUTILS & APPAREIL
		// =====================================================================
		void NkPanneauOutils::OnUI(NkEditorFrameContext &ec) {
			ec.Text("Outil");
			if (ec.Button("Selection")) {
				mM.outil = NkOutil::NK_SELECTION;
			}
			if (ec.Button("Poser")) {
				mM.outil = NkOutil::NK_POSER;
			}
			if (ec.Button("Effacer")) {
				mM.outil = NkOutil::NK_EFFACER;
			}
			const char *nomOutil = (mM.outil == NkOutil::NK_POSER)
									   ? "poser"
									   : ((mM.outil == NkOutil::NK_EFFACER) ? "effacer" : "selection");
			ec.Text(NkString::Format("courant : %s", nomOutil).Data());
			ec.Separator();

			ec.Text("Affichage");
			ec.Checkbox("grille", mM.voirGrille);
			ec.Checkbox("collisionneurs", mM.voirCollisionneurs);
			ec.Separator();

			ec.Text("Simulation");
			ec.Checkbox("physique active", mM.simuler);
			ec.Separator();

			// ── L'appareil simule ────────────────────────────────────────────
			const NkProfilAppareil p = mM.ProfilCourant();
			ec.Text("Appareil simule");
			ec.Text(NkString::Format("%s  %ux%u", p.nom, p.largeur, p.hauteur).Data());
			if (ec.Button("Appareil suivant")) {
				mM.profil = (mM.profil + 1) % NkNbProfils();
			}
			ec.Checkbox("paysage", mM.paysage);

			// ⚠️ Les valeurs affichees sont celles du profil APRES rotation.
			// Afficher celles d'avant donnerait des bandes qui ne correspondent
			// pas a ce qu'on voit dessine -- et on chercherait le defaut dans le
			// dessin, pas dans l'affichage.
			ec.Text(NkString::Format("zone sure  h:%.0f b:%.0f g:%.0f d:%.0f", p.zoneSure.top, p.zoneSure.bottom,
									 p.zoneSure.left, p.zoneSure.right)
						.Data());
			ec.Separator();

			// ⚠️ VUES et DESSINEES, pas « rendus / ecartes » : l ecart entre les
			// deux EST la mesure du hors-champ. Deux compteurs qui ne se
			// soustraient pas ne diraient rien.
			ec.Text(NkString::Format("sprites vus %d / dessines %d", mM.stats.entitesVues,
									 mM.stats.entitesDessinees)
						.Data());
		}

	} // namespace editeur
} // namespace nkentseu
