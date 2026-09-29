//
// NkEditeurDetails.cpp
// =============================================================================
// Description :
//   La colonne de droite, en deux onglets :
//     Details  les COMPOSANTS de l'entite selectionnee (nom, « + Ajouter »,
//              une section repliable par composant, « Retirer »)
//     Monde    les proprietes de la SCENE (gravite, temperature, affichage
//              de la matiere)
//
// Caracteristiques :
//   - Contenu repris tel quel de l'ancien NkEditeurPanneaux.cpp (inspecteur et
//     panneau Monde) : chaque section sait CE QUE le solveur doit recevoir
//     quand on la modifie (ActualiserCorps, Teleporter via Deplacer...).
//   - « + Ajouter » ouvre un MENU DEROULANT de l'editeur (dlOverlay) et non
//     plus une liste en ligne : la liste en ligne existait parce que les popups
//     NKGui d'un panneau ancre ne recevaient pas le clic. Sans shell, le menu
//     de l'editeur n'a pas ce defaut.
//
// ⚠️ ET CE N'EST PAS `NkEditorInspector`, DELIBEREMENT
//   Celui du kit est pilote par NKReflection et n'affiche que des classes
//   REFLECHIES ; les composants d'Unkeny sont des structures nues.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurInterface.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/NkEditorTextField.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

#include <cstdio>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkGuiContext;
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

			/// Une section repliable OUVERTE la premiere fois qu'elle apparait : un
			/// inspecteur dont toutes les sections naissent fermees oblige a
			/// cliquer partout pour voir la moindre valeur. Ensuite, l'etat que
			/// l'utilisateur a choisi est garde (par NKGui, sous le meme id).
			bool Section(NkGuiContext &ctx, const char *libelle) {
				static NkVector<nkgui::NkGuiId> vues;
				const nkgui::NkGuiId id = ctx.GetId(libelle);
				bool nouvelle = true;
				for (uint32 i = 0; i < vues.Size() && nouvelle; ++i) {
					nouvelle = !(vues[i] == id);
				}
				if (nouvelle) {
					vues.PushBack(id);
					ctx.SetNodeOpen(id, true);
				}
				return nkgui::CollapsingHeader(ctx, libelle);
			}

			/// Un choix parmi quelques valeurs, SEGMENTE (UI_SPEC §3.1) : des
			/// boutons sur une ligne, la valeur courante grisee.
			bool Choix(NkGuiContext &ctx, const char *libelle, const char *const *noms, int32 n, int32 &valeur) {
				nkgui::Text(ctx, NkString::Format("%s : %s", libelle, noms[valeur]).CStr());
				bool change = false;
				ctx.PushId(libelle);
				for (int32 i = 0; i < n; ++i) {
					if (i > 0) {
						ctx.SameLine();
					}
					ctx.BeginDisabled(i == valeur);
					if (nkgui::Button(ctx, noms[i])) {
						valeur = i;
						change = true;
					}
					ctx.EndDisabled();
				}
				ctx.PopId();
				return change;
			}

			/// Une ligne « cle : valeur » en lecture seule.
			void Ligne(NkGuiContext &ctx, const char *cle, const NkString &valeur) {
				nkgui::Text(ctx, NkString::Format("%s : %s", cle, valeur.CStr()).CStr());
			}

			/// Un curseur sur une couleur RGBA : les quatre canaux en 0..1.
			bool Couleur(NkGuiContext &ctx, const char *libelle, uint32 &rgba) {
				float32 c[4] = {static_cast<float32>((rgba >> 24) & 0xFFu) / 255.f,
								static_cast<float32>((rgba >> 16) & 0xFFu) / 255.f,
								static_cast<float32>((rgba >> 8) & 0xFFu) / 255.f,
								static_cast<float32>(rgba & 0xFFu) / 255.f};
				if (!nkgui::ColorEdit4(ctx, libelle, c)) {
					return false;
				}
				auto o = [](float32 v) {
					return static_cast<uint32>((v < 0.f ? 0.f : (v > 1.f ? 1.f : v)) * 255.f + 0.5f);
				};
				rgba = (o(c[0]) << 24) | (o(c[1]) << 16) | (o(c[2]) << 8) | o(c[3]);
				return true;
			}

			/// L'en-tete d'une section de composant : son nom, et « Retirer ».
			/// Rend false si le composant vient d'etre retire (ne plus rien lire).
			bool EnTete(NkEditeurCadre &c, ecs::NkEntityId id, NkComposantEditeur comp, bool &ouvert) {
				NkGuiContext &ctx = c.ctx;
				nkgui::Separator(ctx);
				ouvert = Section(ctx, NkComposantEditeurNom(comp));
				if (ouvert) {
					ctx.PushId(NkComposantEditeurNom(comp));
					const bool retirer = nkgui::Button(ctx, "Retirer ce composant");
					ctx.PopId();
					if (retirer) {
						NkEditeurRetirerComposant(c.m, id, comp);
						return false;
					}
				}
				return true;
			}

			void Transform(NkEditeurCadre &c, ecs::NkEntityId id) {
				NkGuiContext &ctx = c.ctx;
				NkTransform2D *t = c.m.scene.Monde().Get<NkTransform2D>(id);
				if (t == nullptr) {
					return;
				}
				nkgui::Separator(ctx);
				if (!Section(ctx, "Transform")) {
					return;
				}
				// ⚠️ La position passe par l'action Deplacer : un rigide est
				// TELEPORTE (sinon le solveur le ramene), un corps mou TRANSLATE.
				NkVec2f centre = t->position;
				NkEditeurCentreSelection(c.m, centre);
				float32 x = centre.x;
				float32 y = centre.y;
				const bool bx = nkgui::DragFloat(ctx, "x (m)", x, 0.02f);
				const bool by = nkgui::DragFloat(ctx, "y (m)", y, 0.02f);
				if (bx || by) {
					NkEditeurDeplacer(c.m, NkVec2f(x, y));
				}
				if (!c.m.scene.Monde().Has<NkCorpsMou2D>(id)) {
					float32 rot = t->rotation * 57.2957795f;
					if (nkgui::SliderFloat(ctx, "rotation (deg)", rot, -180.f, 180.f)) {
						t->rotation = rot / 57.2957795f;
						if (c.m.scene.Monde().Has<NkCorps2D>(id)) {
							c.m.scene.ActualiserCorps(id); // le corps prend la nouvelle orientation
						}
					}
				}
			}

			void Sprite(NkEditeurCadre &c, ecs::NkEntityId id) {
				bool ouvert = false;
				if (!EnTete(c, id, NkComposantEditeur::NK_SPRITE, ouvert) || !ouvert) {
					return;
				}
				NkGuiContext &ctx = c.ctx;
				NkSprite2D *s = c.m.scene.Monde().Get<NkSprite2D>(id);
				nkgui::SliderFloat(ctx, "largeur", s->taille.x, 0.05f, 20.f);
				nkgui::SliderFloat(ctx, "hauteur", s->taille.y, 0.05f, 20.f);
				Couleur(ctx, "teinte", s->couleur);
				nkgui::Checkbox(ctx, "visible", s->visible);
				nkgui::InputInt(ctx, "couche", s->couche);
				Ligne(ctx, "texture", NkString(s->texId != 0u ? c.m.textures.Nom(s->texId) : "(aucune)"));
			}

			void Collisionneur(NkEditeurCadre &c, ecs::NkEntityId id) {
				bool ouvert = false;
				if (!EnTete(c, id, NkComposantEditeur::NK_COLLISIONNEUR, ouvert) || !ouvert) {
					return;
				}
				NkGuiContext &ctx = c.ctx;
				NkCollisionneur2D *col = c.m.scene.Monde().Get<NkCollisionneur2D>(id);
				bool change = false;
				static const char *kFormes[3] = {"cercle", "boite", "capsule"};
				int32 f = static_cast<int32>(col->forme);
				if (Choix(ctx, "forme", kFormes, 3, f)) {
					col->forme = static_cast<NkForme2D>(f);
					change = true;
				}
				if (col->forme == NkForme2D::NK_BOITE) {
					change |= nkgui::SliderFloat(ctx, "demi-largeur", col->demiTaille.x, 0.02f, 10.f);
					change |= nkgui::SliderFloat(ctx, "demi-hauteur", col->demiTaille.y, 0.02f, 10.f);
				} else {
					change |= nkgui::SliderFloat(ctx, "rayon", col->rayon, 0.02f, 5.f);
					if (col->forme == NkForme2D::NK_CAPSULE) {
						change |= nkgui::SliderFloat(ctx, "demi-longueur", col->demiTaille.x, 0.f, 10.f);
					}
				}
				change |= nkgui::Checkbox(ctx, "declencheur (zone)", col->declencheur);
				// ⚠️ La forme vit AUSSI dans le solveur : on refait le corps.
				if (change && c.m.scene.Monde().Has<NkCorps2D>(id)) {
					c.m.scene.ActualiserCorps(id);
				}
			}

			void Corps(NkEditeurCadre &c, ecs::NkEntityId id) {
				bool ouvert = false;
				if (!EnTete(c, id, NkComposantEditeur::NK_CORPS, ouvert) || !ouvert) {
					return;
				}
				NkGuiContext &ctx = c.ctx;
				NkCorps2D *b = c.m.scene.Monde().Get<NkCorps2D>(id);
				bool change = false;
				static const char *kTypes[3] = {"statique", "cinematique", "dynamique"};
				int32 ty = static_cast<int32>(b->type);
				if (Choix(ctx, "type", kTypes, 3, ty)) {
					b->type = static_cast<NkTypeCorps>(ty);
					change = true;
				}
				change |= nkgui::SliderFloat(ctx, "masse (kg)", b->masse, 0.1f, 200.f);
				change |= nkgui::SliderFloat(ctx, "friction", b->friction, 0.f, 1.5f);
				change |= nkgui::SliderFloat(ctx, "rebond", b->rebond, 0.f, 1.f);
				change |= nkgui::SliderFloat(ctx, "gravite (x)", b->echelleGravite, -2.f, 4.f);
				change |= nkgui::SliderFloat(ctx, "frein lineaire", b->amortissementLineaire, 0.f, 5.f);
				change |= nkgui::SliderFloat(ctx, "frein angulaire", b->amortissementAngulaire, 0.f, 5.f);
				change |= nkgui::Checkbox(ctx, "rotation bloquee", b->rotationBloquee);
				// ⚠️ NkCorps2D est la DESCRIPTION ; le corps vit dans NKPhysics.
				// Sans cet appel, les Details montreraient une masse que le
				// solveur n'a pas.
				if (change) {
					c.m.scene.ActualiserCorps(id);
				}
				const NkVec2f v = c.m.scene.Vitesse(id);
				Ligne(ctx, "vitesse",
					  NkString::Format("%.2f m/s", static_cast<double>(math::NkSqrt(v.x * v.x + v.y * v.y))));
			}

			void CorpsMou(NkEditeurCadre &c, ecs::NkEntityId id) {
				bool ouvert = false;
				if (!EnTete(c, id, NkComposantEditeur::NK_CORPS_MOU, ouvert) || !ouvert) {
					return;
				}
				NkGuiContext &ctx = c.ctx;
				NkCorpsMou2D *mou = c.m.scene.Monde().Get<NkCorpsMou2D>(id);
				physics::NkParticules2D *p = c.m.scene.Particules();
				const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
				if (ci < 0) {
					nkgui::Text(ctx, "(matiere disparue)");
					return;
				}
				physics::NkCorpsP2D &corps = p->corps[static_cast<uint32>(ci)];
				Ligne(ctx, "materiau", NkString(physics::NkMateriauP2DNom(corps.mat)));
				Ligne(ctx, "matiere",
					  NkString::Format("%u particules, %u liens", static_cast<unsigned>(corps.nombre),
									   static_cast<unsigned>(p->LiensActifsDuCorps(static_cast<uint32>(ci)))));
				Couleur(ctx, "couleur", mou->couleur);
				nkgui::Checkbox(ctx, "visible", mou->visible);
				// Les parametres du SOLVEUR : lus a chaque sous-pas, donc
				// modifiables a chaud, sans rien refaire.
				nkgui::SliderFloat(ctx, "raideur", corps.raideur, 0.f, 1.f);
				nkgui::SliderFloat(ctx, "friction", corps.friction, 0.f, 1.f);
				nkgui::SliderFloat(ctx, "rebond", corps.rebond, 0.f, 1.f);
				if (corps.mat == physics::NkMateriauP2D::NK_BALLON) {
					nkgui::SliderFloat(ctx, "pression", corps.pression, 0.f, 3.f);
				}
				if (corps.mat == physics::NkMateriauP2D::NK_BLOB || physics::NkEstFluideP2D(corps.mat)) {
					nkgui::SliderFloat(ctx, "viscosite", corps.viscosite, 0.f, 0.5f);
					nkgui::SliderFloat(ctx, "cohesion", corps.cohesion, 0.f, 1.5f);
				}
				if (corps.mat == physics::NkMateriauP2D::NK_BLOB) {
					nkgui::SliderFloat(ctx, "plasticite (1/s)", corps.plasticite, 0.f, 30.f);
				}
				if (corps.mat == physics::NkMateriauP2D::NK_GELEE) {
					nkgui::SliderFloat(ctx, "memoire de forme", corps.formeRaideur, 0.f, 1.f);
				}
				nkgui::SliderFloat(ctx, "resistance (rupture)", corps.resistance, 0.f, 3.f);
				nkgui::Checkbox(ctx, "se touche elle-meme", corps.autoCollision);
				nkgui::Checkbox(ctx, "touche les rigides", corps.couplageRigide);
				if (nkgui::Button(ctx, "Epingler tout")) {
					p->EpinglerCorps(static_cast<uint32>(ci), true);
				}
				ctx.SameLine();
				if (nkgui::Button(ctx, "Liberer")) {
					p->EpinglerCorps(static_cast<uint32>(ci), false);
				}
			}

			void Source(NkEditeurCadre &c, ecs::NkEntityId id) {
				bool ouvert = false;
				if (!EnTete(c, id, NkComposantEditeur::NK_SOURCE, ouvert) || !ouvert) {
					return;
				}
				NkGuiContext &ctx = c.ctx;
				NkSource2D *s = c.m.scene.Monde().Get<NkSource2D>(id);
				nkgui::InputInt(ctx, "son (id)", reinterpret_cast<int32 &>(s->son));
				nkgui::SliderFloat(ctx, "volume", s->volume, 0.f, 2.f);
				nkgui::SliderFloat(ctx, "hauteur", s->pitch, 0.25f, 4.f);
				nkgui::SliderFloat(ctx, "portee hors champ (m)", s->portee, 0.f, 50.f);
				nkgui::Checkbox(ctx, "boucle", s->boucle);
				nkgui::Checkbox(ctx, "au demarrage", s->auDemarrage);
				nkgui::Checkbox(ctx, "spatial", s->spatial);
			}

			void Animation(NkEditeurCadre &c, ecs::NkEntityId id) {
				bool ouvert = false;
				if (!EnTete(c, id, NkComposantEditeur::NK_ANIMATION, ouvert) || !ouvert) {
					return;
				}
				NkGuiContext &ctx = c.ctx;
				NkAnimSprite2D *a = c.m.scene.Monde().Get<NkAnimSprite2D>(id);
				int32 col = a->colonnes;
				int32 lig = a->lignes;
				if (nkgui::InputInt(ctx, "colonnes de l'atlas", col)) {
					a->colonnes = static_cast<uint16>(col < 1 ? 1 : col);
				}
				if (nkgui::InputInt(ctx, "lignes de l'atlas", lig)) {
					a->lignes = static_cast<uint16>(lig < 1 ? 1 : lig);
				}
				if (a->nbClips > 0) {
					NkClipSprite &clip = a->clips[0];
					int32 premiere = clip.premiere;
					int32 nombre = clip.nombre;
					if (nkgui::InputInt(ctx, "clip 0 : premiere image", premiere)) {
						clip.premiere = static_cast<uint16>(premiere < 0 ? 0 : premiere);
					}
					if (nkgui::InputInt(ctx, "clip 0 : images", nombre)) {
						clip.nombre = static_cast<uint16>(nombre < 1 ? 1 : nombre);
					}
					nkgui::SliderFloat(ctx, "images / s", clip.imagesParSeconde, 0.f, 60.f);
				}
				nkgui::Checkbox(ctx, "en pause", a->enPause);
			}

			/// Le champ Nom, en haut des Details. Le renommage est IMMEDIAT : il
			/// n'y a pas de « valider » a oublier, l'Outliner suit a la frappe.
			void ChampNom(NkEditeurCadre &c, ecs::NkEntityId id, const NkRect &r) {
				NkEditeurInterface &ui = c.ui;
				const nkgui::NkGuiInput &in = c.ctx.input;
				// Le tampon suit l'entite choisie.
				if (!(ui.nomDe == id)) {
					ui.nomDe = id;
					std::snprintf(ui.nom, sizeof(ui.nom), "%s", NomDe(c.m.scene, id).CStr());
					ui.nomFocus = false;
				}
				if (in.mouseClicked[0]) {
					ui.nomFocus = NkEditeurDans(r, in.mousePos);
				}
				if (ui.nomFocus && (in.KeyPressed(nkgui::NkGuiKey::Enter) || in.KeyPressed(nkgui::NkGuiKey::Escape))) {
					ui.nomFocus = false;
				}
				c.ctx.dl.AddRectFilled(r, c.pal.champ, 2.f);
				c.ctx.dl.AddRect(r, ui.nomFocus ? c.pal.accent : c.pal.bord, 1.f, 2.f);
				editorkit::NkOverlayFieldStyle st;
				st.fond = false;
				st.bord = false;
				st.texte = c.pal.texte;
				const NkRect champ{r.x + 6.f, r.y, r.w - 8.f, r.h};
				editorkit::NkOverlayTextField(c.ctx, c.ctx.dl, c.police, champ, ui.nom, static_cast<int32>(sizeof(ui.nom)),
											  ui.nomFocus, &st);
				if (ui.nom[0] != '\0' && !(NomDe(c.m.scene, id) == NkString(ui.nom))) {
					NkEditeurRenommer(c.m, id, ui.nom);
				}
			}

			void OngletDetails(NkEditeurCadre &c, const NkRect &zone) {
				NkGuiContext &ctx = c.ctx;
				auto &dl = ctx.dl;
				if (!c.m.aSelection || !c.m.scene.Monde().IsAlive(c.m.selection)) {
					// Un etat vide qui PARLE (UI_SPEC §0, regle 3) : il dit quoi faire.
					const float32 lh = renderer::NkTexteHauteurLigne(c.police, 16.f);
					const float32 cy = zone.y + zone.h * 0.35f;
					renderer::NkTexteCentre(dl, c.police, zone.x + zone.w * 0.5f, cy,
											"Sélectionnez une entité pour voir ses détails.", c.pal.attenue);
					renderer::NkTexteCentre(dl, c.petite, zone.x + zone.w * 0.5f, cy + lh + 6.f,
											"Cliquez-la dans la vue ou l'Outliner, ou créez-en une (Ctrl+E).",
											c.pal.attenue);
					return;
				}
				const ecs::NkEntityId id = c.m.selection;

				// ── La rangee du haut : [ Nom ][ + Ajouter ] ─────────────────
				const float32 rangeeH = 24.f;
				const float32 ajoutW = renderer::NkTexteLargeur(c.police, "+ Ajouter") + 20.f;
				const NkRect nomR{zone.x + 6.f, zone.y + 6.f, zone.w - ajoutW - 18.f, rangeeH};
				ChampNom(c, id, nomR);
				const NkRect ajout{nomR.x + nomR.w + 6.f, nomR.y, ajoutW, rangeeH};
				const bool ouvert = c.ui.menu == NkMenuEditeur::NK_COMPOSANT;
				if (NkEditeurBouton(c, ajout, "+ Ajouter", ouvert)) {
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_COMPOSANT, ajout);
				}
				const NkString type = NkString::Format("type : %s", NkEditeurTypeDe(c.m.scene, id));
				renderer::NkTexte(dl, c.petite, zone.x + 8.f, nomR.y + rangeeH + 5.f, type.CStr(), c.pal.attenue);

				// ── Les sections, dans une zone defilable ────────────────────
				const float32 haut = nomR.y + rangeeH + 24.f;
				const NkRect corps{zone.x, haut, zone.w, zone.y + zone.h - haut};
				if (!nkgui::BeginChild(ctx, "details.composants", corps, false)) {
					return;
				}
				// Chaque entite garde ses propres sections : sans cet id, deplier le
				// Sprite d'une caisse le deplierait pour toutes.
				char cle[24];
				std::snprintf(cle, sizeof(cle), "e%llu", static_cast<unsigned long long>(id.Pack()));
				ctx.PushId(cle);
				Transform(c, id);
				if (c.m.scene.Monde().Has<NkSprite2D>(id)) {
					Sprite(c, id);
				}
				if (c.m.scene.Monde().Has<NkCollisionneur2D>(id)) {
					Collisionneur(c, id);
				}
				if (c.m.scene.Monde().Has<NkCorps2D>(id)) {
					Corps(c, id);
				}
				if (c.m.scene.Monde().Has<NkCorpsMou2D>(id)) {
					CorpsMou(c, id);
				}
				if (c.m.scene.Monde().Has<NkSource2D>(id)) {
					Source(c, id);
				}
				if (c.m.scene.Monde().Has<NkAnimSprite2D>(id)) {
					Animation(c, id);
				}
				ctx.PopId();
				nkgui::EndChild(ctx);
			}

			void OngletMonde(NkEditeurCadre &c, const NkRect &zone) {
				NkGuiContext &ctx = c.ctx;
				NkEditeurModele &m = c.m;
				const NkRect corps{zone.x, zone.y + 4.f, zone.w, zone.h - 4.f};
				if (!nkgui::BeginChild(ctx, "details.monde", corps, false)) {
					return;
				}
				physics::NkParticules2D *p = m.scene.Particules();
				nkgui::Text(ctx, "Propriétés de la scène");
				nkgui::Separator(ctx);
				if (p != nullptr) {
					physics::NkReglagesP2D &r = p->reglages;
					// La gravite est UNE : celle des rigides suit celle des particules.
					float32 g = r.gravite.y;
					if (nkgui::SliderFloat(ctx, "gravite (m/s2)", g, -30.f, 10.f)) {
						r.gravite.y = g;
						if (m.scene.MondePhysique() != nullptr) {
							m.scene.MondePhysique()->SetGravity(math::NkVec3f(r.gravite.x, g, 0.f));
						}
					}
					nkgui::SliderFloat(ctx, "temperature", r.temperature, 0.f, 100.f);
					nkgui::SliderFloat(ctx, "vent (m/s2)", r.vent, -20.f, 20.f);
					nkgui::SliderFloat(ctx, "frein de l'air (1/s)", r.amortAir, 0.f, 3.f);
					nkgui::InputInt(ctx, "sous-pas", r.sousPas);
					r.sousPas = r.sousPas < 1 ? 1 : (r.sousPas > 32 ? 32 : r.sousPas);
					nkgui::Checkbox(ctx, "collisions de la matiere", r.collisions);
					nkgui::Checkbox(ctx, "soudure (atomes, blobs)", r.soudure);
					nkgui::Checkbox(ctx, "boite de la matiere", r.limites.actif);
					nkgui::Separator(ctx);
					Ligne(ctx, "matiere",
						  NkString::Format("%u particules, %u liens, %u ruptures", static_cast<unsigned>(p->particules.Size()),
										   static_cast<unsigned>(p->LiensActifs()), static_cast<unsigned>(p->rupturesTotal)));
				} else {
					nkgui::Text(ctx, "(cette scene n'a pas de matiere)");
				}
				Ligne(ctx, "pas fixe",
					  NkString::Format("%.4f s (%d max par trame)", static_cast<double>(m.scene.Config().pasFixe),
									   m.scene.Config().pasMaxParTrame));
				nkgui::Separator(ctx);
				nkgui::Text(ctx, "Affichage de la matière");
				static const char *kModes[4] = {"Eclaire", "Filaire", "Contraintes", "Vitesse"};
				int32 mode = static_cast<int32>(m.rendu.mode);
				if (Choix(ctx, "mode", kModes, 4, mode)) {
					m.rendu.mode = static_cast<NkModeRenduParticules>(mode);
				}
				nkgui::Checkbox(ctx, "liens", m.rendu.liens);
				nkgui::Checkbox(ctx, "particules", m.rendu.particules);
				nkgui::Checkbox(ctx, "vitesses", m.rendu.vitesses);
				nkgui::Separator(ctx);
				// L'appareil simule : ce que la zone sure du viseur represente.
				const NkProfilAppareil pa = m.ProfilCourant();
				nkgui::Text(ctx, "Appareil simulé");
				nkgui::Text(ctx, NkString::Format("%s  %ux%u", pa.nom, pa.largeur, pa.hauteur).CStr());
				nkgui::Text(ctx, NkString::Format("zone sure  h:%.0f b:%.0f g:%.0f d:%.0f", pa.zoneSure.top,
												  pa.zoneSure.bottom, pa.zoneSure.left, pa.zoneSure.right)
									 .CStr());
				nkgui::EndChild(ctx);
			}

		} // namespace

		void NkEditeurDessinerDetails(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			const NkRect zone = ui.details;
			if (!ui.voirDetails || zone.w < 8.f || zone.h < 60.f) {
				return;
			}
			c.ctx.dl.AddRectFilled(zone, c.pal.panneau);
			static const char *kOnglets[2] = {"Détails", "Monde"};
			const float32 ongletsH = 26.f;
			NkEditeurOnglets(c, NkRect{zone.x, zone.y, zone.w, ongletsH}, kOnglets, 2, ui.ongletDroite);
			const NkRect contenu{zone.x, zone.y + ongletsH, zone.w, zone.h - ongletsH};
			c.ctx.dl.PushClipRect(contenu, true);
			if (ui.ongletDroite == 0) {
				OngletDetails(c, contenu);
			} else {
				OngletMonde(c, contenu);
			}
			c.ctx.dl.PopClipRect();
		}

	} // namespace editeur
} // namespace nkentseu
