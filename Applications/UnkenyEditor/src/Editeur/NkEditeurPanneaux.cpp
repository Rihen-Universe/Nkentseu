// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurPanneaux.cpp
// DESCRIPTION: Les panneaux de l'editeur, en NkEditorPanel (NKEditorKit).
//
// ⚠️ LES PANNEAUX N'ONT PAS DE LOGIQUE A EUX (2026-09-29)
//   Chaque geste appelle une action de NkEditeurActions.h. C'est ce qui permet
//   au banc (`--selftest`) d'eprouver l'editeur sans fenetre : ce qui se
//   verifie la-bas est exactement ce que fait un clic ici.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurPanneaux.h"
#include "Editeur/NkEditeurViseur.h"
#include "NKContainers/String/NkString.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

#include <cstdio>

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

			/// Une section repliable OUVERTE la premiere fois qu'elle apparait : un
			/// inspecteur dont toutes les sections naissent fermees oblige a
			/// cliquer partout pour voir la moindre valeur. Ensuite, l'etat que
			/// l'utilisateur a choisi est garde (par NKGui, sous le meme id).
			bool Section(nkgui::NkGuiContext &ctx, const char *libelle) {
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

			/// Un choix parmi quelques valeurs, en BOUTONS sur une ligne (la valeur
			/// courante est grisee). Pas de liste deroulante : voir Ajouter().
			bool Choix(NkEditorFrameContext &ec, const char *libelle, const char *const *noms, int32 n, int32 &valeur) {
				auto &ctx = ec.Ui();
				ec.Text(NkString::Format("%s : %s", libelle, noms[valeur]).Data());
				bool change = false;
				ctx.PushId(libelle);
				for (int32 i = 0; i < n; ++i) {
					if (i > 0) {
						ctx.SameLine();
					}
					ctx.BeginDisabled(i == valeur);
					if (ec.Button(noms[i])) {
						valeur = i;
						change = true;
					}
					ctx.EndDisabled();
				}
				ctx.PopId();
				return change;
			}

			/// Une ligne « cle : valeur » en lecture seule.
			void Ligne(NkEditorFrameContext &ec, const char *cle, const NkString &valeur) {
				ec.Text(NkString::Format("%s : %s", cle, valeur.Data()).Data());
			}

			/// Un curseur sur une couleur RGBA : les quatre canaux en 0..1.
			bool Couleur(NkEditorFrameContext &ec, const char *libelle, uint32 &rgba) {
				float32 c[4] = {static_cast<float32>((rgba >> 24) & 0xFFu) / 255.f, static_cast<float32>((rgba >> 16) & 0xFFu) / 255.f,
								static_cast<float32>((rgba >> 8) & 0xFFu) / 255.f, static_cast<float32>(rgba & 0xFFu) / 255.f};
				if (!nkgui::ColorEdit4(ec.Ui(), libelle, c)) {
					return false;
				}
				auto o = [](float32 v) { return static_cast<uint32>((v < 0.f ? 0.f : (v > 1.f ? 1.f : v)) * 255.f + 0.5f); };
				rgba = (o(c[0]) << 24) | (o(c[1]) << 16) | (o(c[2]) << 8) | o(c[3]);
				return true;
			}
		} // namespace

		// =====================================================================
		// LA BARRE D'OUTILS
		// =====================================================================
		void NkBarreOutilsEditeur(NkEditorFrameContext &ec, void *user) {
			if (user == nullptr) {
				return;
			}
			NkEditeurModele &m = *static_cast<NkEditeurModele *>(user);
			auto &ctx = ec.Ui();
			const bool joue = m.etat == NkEtatJeu::NK_JEU;
			if (ec.Button(joue ? "Pause" : "Jouer")) {
				joue ? NkEditeurPause(m) : NkEditeurJouer(m);
			}
			ctx.SameLine();
			if (ec.Button("Pas")) {
				NkEditeurUnPas(m);
			}
			ctx.SameLine();
			ctx.BeginDisabled(m.etat == NkEtatJeu::NK_EDITION);
			if (ec.Button("Arreter")) {
				NkEditeurArreter(m);
			}
			ctx.EndDisabled();
			ctx.SameLine(24.f);
			if (ec.Button("Nouveau")) {
				NkEditeurNouvelleScene(m);
				NkEditeurAnnoncer(m, "Nouvelle scene");
			}
			ctx.SameLine();
			if (ec.Button("Ouvrir")) {
				NkEditeurOuvrir(m);
			}
			ctx.SameLine();
			if (ec.Button("Enregistrer")) {
				NkEditeurSauver(m);
			}
			ctx.SameLine(24.f);
			const char *etat = joue ? "EN JEU" : (m.etat == NkEtatJeu::NK_PAUSE ? "EN PAUSE" : "EDITION");
			ec.Text(etat);
			if (m.messageAge < 3.f && !m.message.Empty()) {
				ctx.SameLine(24.f);
				ec.Text(m.message.Data());
			}
		}

		// =====================================================================
		// LE VISEUR
		// =====================================================================
		void NkPanneauViseur::OnUI(NkEditorFrameContext &ec) {
			auto &ctx = ec.Ui();
			// L'aire du viseur = tout ce qui reste sous le curseur du panneau.
			// ⚠️ On la redemande A CHAQUE TRAME : avec l'ancrage, un panneau
			// change de taille quand on deplace une cloison, et une aire retenue
			// d'une trame sur l'autre ferait diverger le dessin de la souris.
			// ⚠️ ET BORNEE PAR LA DECOUPE. Dans un panneau ancre, AvailHeight() est
			// la hauteur du CONTENU defilable (~1e9), pas la partie visible : le
			// viseur faisait un milliard de pixels de haut, l'aire d'appareil se
			// centrait bien au-dessous de l'ecran, et l'on ne voyait que le fond.
			// (Mesure du 2026-09-29 sous Xvfb ; NKCode borne de la meme facon.)
			const NkRect clip = ctx.DL().CurrentClip();
			NkRect aire = ctx.NextItemRect(-1.f, ctx.AvailHeight());
			if (aire.y + aire.h > clip.y + clip.h) {
				aire.h = clip.y + clip.h - aire.y;
			}
			if (aire.x + aire.w > clip.x + clip.w) {
				aire.w = clip.x + clip.w - aire.x;
			}
			if (aire.w < 4.f || aire.h < 4.f) {
				return; // panneau reduit a rien : il n'y a rien a dessiner
			}
			const NkRect appareil = NkAireAppareil(aire, mM.ProfilCourant());
			// ⚠️ Le viseur de la CAMERA est l'aire d'appareil, PAS le panneau.
			mM.scene.Camera().PoserViseur(appareil);
			if (mCadrageEnAttente) {
				mCadrageEnAttente = false;
				mM.scene.Camera().Cadrer(NkVec2f(0.f, 0.f), NkVec2f(24.f, 11.f));
			}

			// ── LE PAS DE SIMULATION VIT ICI, ET IL FAUT SAVOIR POURQUOI ────
			// `NkEditorShell::Run()` est une boucle BLOQUANTE : le seul endroit
			// ou du temps s'ecoule et ou l'on recoit un `dt` est le dessin d'un
			// panneau. Fermer le viseur MET donc la simulation en pause.
			NkEditeurAvancer(mM, ec.dt);
			mM.stats = NkDessinerViseur(ctx.DL(), mM, aire, appareil);
			Souris(ec, aire);
		}

		// ---------------------------------------------------------------------
		// La souris. `ctx.InputHits` plutot qu'un test de rectangle : il refuse
		// un point recouvert par une couche superieure (modale, popup).
		// ---------------------------------------------------------------------
		void NkPanneauViseur::Souris(NkEditorFrameContext &ec, const NkRect &aire) {
			auto &ctx = ec.Ui();
			const auto &in = ctx.input;
			const NkVec2f pos(in.mousePos.x, in.mousePos.y);
			const bool dedans = ctx.InputHits(aire);
			NkVue2D &cam = mM.scene.Camera();
			const NkVec2f monde = cam.EcranVersMonde(pos);
			physics::NkParticules2D *p = mM.scene.Particules();

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
				mM.panoramique = true;
				mM.dernierPointeur = pos;
			}
			if (mM.panoramique && (in.mouseDown[1] || in.mouseDown[2] || in.mouseDown[0])) {
				const NkVec2f avant = cam.EcranVersMonde(mM.dernierPointeur);
				cam.PoserCentre(cam.Centre() - (monde - avant));
				mM.dernierPointeur = pos;
			}

			// ── Appui ────────────────────────────────────────────────────────
			if (in.mouseClicked[0] && dedans) {
				mM.dernierPointeur = pos;
				mPrecMonde = monde;
				switch (mM.outil) {
					case NkOutil::NK_POSER:
						if (!mM.acteurSimple && NkActeurSimInfo(mM.acteur).pinceau && mM.etat == NkEtatJeu::NK_JEU) {
							// Un fluide en JEU se VERSE tant qu'on maintient.
							mM.pinceau = NkOuvrirPinceauSim(mM.scene, mM.acteur);
							mGeste = mM.pinceau.IsValid();
						} else {
							NkEditeurPoser(mM, monde);
						}
						break;
					case NkOutil::NK_EFFACER:
						NkEditeurEffacerSous(mM, monde);
						break;
					case NkOutil::NK_SAISIR:
						mGeste = p != nullptr && p->SaisirDebut(monde, 0.6f);
						if (!mGeste) {
							NkVec2f c;
							if (NkEditeurChoisirSous(mM, monde, &c)) {
								mM.deplace = true;
								mM.decalageSaisie = c - monde;
							}
						}
						break;
					case NkOutil::NK_COUTEAU:
						mGeste = true;
						break;
					default: { // NK_SELECTION
						NkVec2f c;
						if (NkEditeurChoisirSous(mM, monde, &c)) {
							mM.deplace = true;
							// On retient le DECALAGE : sans lui, l'entite saute pour
							// se centrer sous le curseur des le premier pixel.
							mM.decalageSaisie = c - monde;
						} else {
							mM.panoramique = true; // clic dans le vide = on deplace la vue
						}
						break;
					}
				}
				return;
			}

			// ── Glissement ───────────────────────────────────────────────────
			// Pas de test d'occultation : une saisie commencee se poursuit meme
			// si le curseur sort du panneau.
			if (in.mouseDown[0]) {
				if (mGeste && mM.outil == NkOutil::NK_SAISIR && p != nullptr) {
					p->SaisirVers(monde);
				} else if (mGeste && mM.outil == NkOutil::NK_COUTEAU && p != nullptr) {
					p->Couper(mPrecMonde, monde);
				} else if (mGeste && mM.outil == NkOutil::NK_POSER) {
					if (!NkVerserSim(mM.scene, mM.pinceau, monde, 3, mM.graine)) {
						mM.pinceau = NkOuvrirPinceauSim(mM.scene, mM.acteur);
					}
				} else if (mM.deplace && mM.aSelection) {
					NkEditeurDeplacer(mM, monde + mM.decalageSaisie);
				}
				mPrecMonde = monde;
				return;
			}

			// ── Relachement ──────────────────────────────────────────────────
			if (in.mouseReleased[0] || in.mouseReleased[1] || in.mouseReleased[2]) {
				if (p != nullptr) {
					p->SaisirFin();
				}
				mGeste = false;
				mM.deplace = false;
				mM.panoramique = false;
				mM.pinceau = ecs::NkEntityId::Invalid();
			}
		}

		void NkPanneauViseur::CadrerSurTout() noexcept {
			mCadrageEnAttente = true;
		}

		// =====================================================================
		// LA HIERARCHIE — la scene et ses entites
		// =====================================================================
		void NkPanneauHierarchie::OnUI(NkEditorFrameContext &ec) {
			auto &ctx = ec.Ui();
			NkVector<ecs::NkEntityId> ids;
			mM.scene.Entites(ids);
			ec.Text(NkString::Format("Scene  -  %u entite(s)", static_cast<uint32>(ids.Size())).Data());
			if (ec.Button("+ Entite")) {
				// Au centre de la vue : la ou l'on regarde.
				NkEditeurCreerEntite(mM, "Entite", mM.scene.Camera().Centre());
			}
			ctx.SameLine();
			ctx.BeginDisabled(!mM.aSelection);
			if (ec.Button("Dupliquer")) {
				NkEditeurDupliquer(mM);
			}
			ctx.SameLine();
			if (ec.Button("Supprimer")) {
				NkEditeurSupprimerSelection(mM);
			}
			ctx.EndDisabled();
			ec.Separator();
			for (uint32 i = 0; i < ids.Size(); ++i) {
				const ecs::NkEntityId id = ids[i];
				// ⚠️ `NkEntityId` porte SON operator== : index ET generation.
				const bool choisie = mM.aSelection && mM.selection == id;
				const NkString libelle = NkString::Format("%s   [%s]", NomDe(mM.scene, id).Data(), NkEditeurTypeDe(mM.scene, id));
				char cle[24];
				std::snprintf(cle, sizeof(cle), "e%llu", static_cast<unsigned long long>(id.Pack()));
				ctx.PushId(cle); // deux entites de meme nom restent deux lignes distinctes
				if (nkgui::Selectable(ctx, libelle.Data(), choisie)) {
					mM.selection = id;
					mM.aSelection = true;
				}
				ctx.PopId();
			}
		}

		// =====================================================================
		// PLACER DES ACTEURS — le catalogue d'Unkeny/Simulation
		// =====================================================================
		void NkPanneauActeurs::OnUI(NkEditorFrameContext &ec) {
			auto &ctx = ec.Ui();
			ec.Text("Cliquer un acteur, puis dans le viseur.");
			ec.Text("Chacun est une entite avec ses composants,");
			ec.Text("modifiables ensuite dans l'Inspecteur.");
			ec.Separator();
			if (nkgui::Selectable(ctx, "Entite simple (sprite + boite)", mM.outil == NkOutil::NK_POSER && mM.acteurSimple)) {
				mM.acteurSimple = true;
				mM.outil = NkOutil::NK_POSER;
			}
			for (int32 k = 0; k < static_cast<int32>(NkCategorieActeur::NK_COUNT); ++k) {
				const NkCategorieActeur cat = static_cast<NkCategorieActeur>(k);
				if (!Section(ctx, NkCategorieActeurNom(cat))) {
					continue;
				}
				for (int32 i = 0; i < static_cast<int32>(NkActeurSim::NK_COUNT); ++i) {
					const NkActeurSim a = static_cast<NkActeurSim>(i);
					const NkInfoActeurSim &info = NkActeurSimInfo(a);
					if (info.categorie != cat) {
						continue;
					}
					const bool arme = mM.outil == NkOutil::NK_POSER && !mM.acteurSimple && mM.acteur == a;
					if (nkgui::Selectable(ctx, info.nom, arme)) {
						mM.acteur = a;
						mM.acteurSimple = false;
						mM.outil = NkOutil::NK_POSER;
					}
					if (ctx.IsItemHovered()) {
						nkgui::SetTooltip(ctx, info.description);
					}
				}
			}
			if (mM.outil == NkOutil::NK_POSER && !mM.acteurSimple) {
				ec.Separator();
				ec.Text(NkActeurSimInfo(mM.acteur).description);
			}
		}

		// =====================================================================
		// L'INSPECTEUR — les COMPOSANTS de l'entite selectionnee
		//
		// ⚠️ ET CE N'EST PAS `NkEditorInspector`, DELIBEREMENT. Celui du kit est
		//    pilote par NKReflection et n'affiche que des classes REFLECHIES ;
		//    les composants d'Unkeny sont des structures nues. Chaque section est
		//    donc ecrite a la main — et chacune sait CE QUE le solveur doit
		//    recevoir quand on la modifie (ActualiserCorps, Teleporter...).
		// =====================================================================
		bool NkPanneauInspecteur::EnTete(NkEditorFrameContext &ec, ecs::NkEntityId id, NkComposantEditeur c, bool &ouvert) {
			auto &ctx = ec.Ui();
			ec.Separator();
			ouvert = Section(ctx, NkComposantEditeurNom(c));
			if (ouvert) {
				ctx.PushId(NkComposantEditeurNom(c));
				const bool retirer = ec.Button("Retirer ce composant");
				ctx.PopId();
				if (retirer) {
					NkEditeurRetirerComposant(mM, id, c);
					return false;
				}
			}
			return true;
		}

		void NkPanneauInspecteur::OnUI(NkEditorFrameContext &ec) {
			auto &ctx = ec.Ui();
			if (!mM.aSelection || !mM.scene.Monde().IsAlive(mM.selection)) {
				ec.Text("Aucune selection.");
				ec.Text("Cliquez une entite dans le viseur ou la");
				ec.Text("hierarchie, ou creez-en une (+ Entite).");
				return;
			}
			const ecs::NkEntityId id = mM.selection;
			// Le nom, modifiable. Le tampon suit l'entite choisie.
			if (!(mNomDe == id)) {
				mNomDe = id;
				std::snprintf(mNom, sizeof(mNom), "%s", NomDe(mM.scene, id).Data());
			}
			if (nkgui::InputText(ctx, "Nom", mNom, static_cast<int32>(sizeof(mNom)))) {
				NkEditeurRenommer(mM, id, mNom);
			}
			ec.Text(NkString::Format("type : %s", NkEditeurTypeDe(mM.scene, id)).Data());

			Transform(ec, id);
			if (mM.scene.Monde().Has<NkSprite2D>(id)) {
				Sprite(ec, id);
			}
			if (mM.scene.Monde().Has<NkCollisionneur2D>(id)) {
				Collisionneur(ec, id);
			}
			if (mM.scene.Monde().Has<NkCorps2D>(id)) {
				Corps(ec, id);
			}
			if (mM.scene.Monde().Has<NkCorpsMou2D>(id)) {
				CorpsMou(ec, id);
			}
			if (mM.scene.Monde().Has<NkSource2D>(id)) {
				Source(ec, id);
			}
			if (mM.scene.Monde().Has<NkAnimSprite2D>(id)) {
				Animation(ec, id);
			}
			if (mM.scene.Monde().IsAlive(id)) {
				Ajouter(ec, id);
			}
		}

		void NkPanneauInspecteur::Transform(NkEditorFrameContext &ec, ecs::NkEntityId id) {
			NkTransform2D *t = mM.scene.Monde().Get<NkTransform2D>(id);
			if (t == nullptr) {
				return;
			}
			ec.Separator();
			if (!Section(ec.Ui(), "Transform")) {
				return;
			}
			// ⚠️ La position passe par l'action Deplacer : un rigide est
			// TELEPORTE (sinon le solveur le ramene), un corps mou TRANSLATE.
			NkVec2f c = t->position;
			NkEditeurCentreSelection(mM, c);
			float32 x = c.x, y = c.y;
			const bool bx = nkgui::DragFloat(ec.Ui(), "x (m)", x, 0.02f);
			const bool by = nkgui::DragFloat(ec.Ui(), "y (m)", y, 0.02f);
			if (bx || by) {
				NkEditeurDeplacer(mM, NkVec2f(x, y));
			}
			if (!mM.scene.Monde().Has<NkCorpsMou2D>(id)) {
				float32 rot = t->rotation * 57.2957795f;
				if (ec.SliderFloat("rotation (deg)", rot, -180.f, 180.f)) {
					t->rotation = rot / 57.2957795f;
					if (mM.scene.Monde().Has<NkCorps2D>(id)) {
						mM.scene.ActualiserCorps(id); // le corps prend la nouvelle orientation
					}
				}
			}
		}

		void NkPanneauInspecteur::Sprite(NkEditorFrameContext &ec, ecs::NkEntityId id) {
			bool ouvert = false;
			if (!EnTete(ec, id, NkComposantEditeur::NK_SPRITE, ouvert) || !ouvert) {
				return;
			}
			NkSprite2D *s = mM.scene.Monde().Get<NkSprite2D>(id);
			ec.SliderFloat("largeur", s->taille.x, 0.05f, 20.f);
			ec.SliderFloat("hauteur", s->taille.y, 0.05f, 20.f);
			Couleur(ec, "teinte", s->couleur);
			ec.Checkbox("visible", s->visible);
			nkgui::InputInt(ec.Ui(), "couche", s->couche);
			Ligne(ec, "texture", NkString(s->texId != 0u ? mM.textures.Nom(s->texId) : "(aucune)"));
		}

		void NkPanneauInspecteur::Collisionneur(NkEditorFrameContext &ec, ecs::NkEntityId id) {
			bool ouvert = false;
			if (!EnTete(ec, id, NkComposantEditeur::NK_COLLISIONNEUR, ouvert) || !ouvert) {
				return;
			}
			NkCollisionneur2D *c = mM.scene.Monde().Get<NkCollisionneur2D>(id);
			bool change = false;
			static const char *kFormes[3] = {"cercle", "boite", "capsule"};
			int32 f = static_cast<int32>(c->forme);
			if (Choix(ec, "forme", kFormes, 3, f)) {
				c->forme = static_cast<NkForme2D>(f);
				change = true;
			}
			if (c->forme == NkForme2D::NK_BOITE) {
				change |= ec.SliderFloat("demi-largeur", c->demiTaille.x, 0.02f, 10.f);
				change |= ec.SliderFloat("demi-hauteur", c->demiTaille.y, 0.02f, 10.f);
			} else {
				change |= ec.SliderFloat("rayon", c->rayon, 0.02f, 5.f);
				if (c->forme == NkForme2D::NK_CAPSULE) {
					change |= ec.SliderFloat("demi-longueur", c->demiTaille.x, 0.f, 10.f);
				}
			}
			change |= ec.Checkbox("declencheur (zone)", c->declencheur);
			// ⚠️ La forme vit AUSSI dans le solveur : on refait le corps.
			if (change && mM.scene.Monde().Has<NkCorps2D>(id)) {
				mM.scene.ActualiserCorps(id);
			}
		}

		void NkPanneauInspecteur::Corps(NkEditorFrameContext &ec, ecs::NkEntityId id) {
			bool ouvert = false;
			if (!EnTete(ec, id, NkComposantEditeur::NK_CORPS, ouvert) || !ouvert) {
				return;
			}
			NkCorps2D *b = mM.scene.Monde().Get<NkCorps2D>(id);
			bool change = false;
			static const char *kTypes[3] = {"statique", "cinematique", "dynamique"};
			int32 ty = static_cast<int32>(b->type);
			if (Choix(ec, "type", kTypes, 3, ty)) {
				b->type = static_cast<NkTypeCorps>(ty);
				change = true;
			}
			change |= ec.SliderFloat("masse (kg)", b->masse, 0.1f, 200.f);
			change |= ec.SliderFloat("friction", b->friction, 0.f, 1.5f);
			change |= ec.SliderFloat("rebond", b->rebond, 0.f, 1.f);
			change |= ec.SliderFloat("gravite (x)", b->echelleGravite, -2.f, 4.f);
			change |= ec.SliderFloat("frein lineaire", b->amortissementLineaire, 0.f, 5.f);
			change |= ec.SliderFloat("frein angulaire", b->amortissementAngulaire, 0.f, 5.f);
			change |= ec.Checkbox("rotation bloquee", b->rotationBloquee);
			// ⚠️ NkCorps2D est la DESCRIPTION ; le corps vit dans NKPhysics.
			// Sans cet appel, l'inspecteur montrerait une masse que le solveur
			// n'a pas.
			if (change) {
				mM.scene.ActualiserCorps(id);
			}
			const NkVec2f v = mM.scene.Vitesse(id);
			Ligne(ec, "vitesse", NkString::Format("%.2f m/s", static_cast<double>(math::NkSqrt(v.x * v.x + v.y * v.y))));
		}

		void NkPanneauInspecteur::CorpsMou(NkEditorFrameContext &ec, ecs::NkEntityId id) {
			bool ouvert = false;
			if (!EnTete(ec, id, NkComposantEditeur::NK_CORPS_MOU, ouvert) || !ouvert) {
				return;
			}
			NkCorpsMou2D *mou = mM.scene.Monde().Get<NkCorpsMou2D>(id);
			physics::NkParticules2D *p = mM.scene.Particules();
			const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
			if (ci < 0) {
				ec.Text("(matiere disparue)");
				return;
			}
			physics::NkCorpsP2D &c = p->corps[static_cast<uint32>(ci)];
			Ligne(ec, "materiau", NkString(physics::NkMateriauP2DNom(c.mat)));
			Ligne(ec, "matiere", NkString::Format("%u particules, %u liens", static_cast<unsigned>(c.nombre),
												   static_cast<unsigned>(p->LiensActifsDuCorps(static_cast<uint32>(ci)))));
			Couleur(ec, "couleur", mou->couleur);
			ec.Checkbox("visible", mou->visible);
			// Les parametres du SOLVEUR : lus a chaque sous-pas, donc modifiables
			// a chaud, sans rien refaire.
			ec.SliderFloat("raideur", c.raideur, 0.f, 1.f);
			ec.SliderFloat("friction", c.friction, 0.f, 1.f);
			ec.SliderFloat("rebond", c.rebond, 0.f, 1.f);
			if (c.mat == physics::NkMateriauP2D::NK_BALLON) {
				ec.SliderFloat("pression", c.pression, 0.f, 3.f);
			}
			if (c.mat == physics::NkMateriauP2D::NK_BLOB || physics::NkEstFluideP2D(c.mat)) {
				ec.SliderFloat("viscosite", c.viscosite, 0.f, 0.5f);
				ec.SliderFloat("cohesion", c.cohesion, 0.f, 1.5f);
			}
			if (c.mat == physics::NkMateriauP2D::NK_BLOB) {
				ec.SliderFloat("plasticite (1/s)", c.plasticite, 0.f, 30.f);
			}
			if (c.mat == physics::NkMateriauP2D::NK_GELEE) {
				ec.SliderFloat("memoire de forme", c.formeRaideur, 0.f, 1.f);
			}
			ec.SliderFloat("resistance (rupture)", c.resistance, 0.f, 3.f);
			ec.Checkbox("se touche elle-meme", c.autoCollision);
			ec.Checkbox("touche les rigides", c.couplageRigide);
			if (ec.Button("Epingler tout")) {
				p->EpinglerCorps(static_cast<uint32>(ci), true);
			}
			ec.Ui().SameLine();
			if (ec.Button("Liberer")) {
				p->EpinglerCorps(static_cast<uint32>(ci), false);
			}
		}

		void NkPanneauInspecteur::Source(NkEditorFrameContext &ec, ecs::NkEntityId id) {
			bool ouvert = false;
			if (!EnTete(ec, id, NkComposantEditeur::NK_SOURCE, ouvert) || !ouvert) {
				return;
			}
			NkSource2D *s = mM.scene.Monde().Get<NkSource2D>(id);
			nkgui::InputInt(ec.Ui(), "son (id)", reinterpret_cast<int32 &>(s->son));
			ec.SliderFloat("volume", s->volume, 0.f, 2.f);
			ec.SliderFloat("hauteur", s->pitch, 0.25f, 4.f);
			ec.SliderFloat("portee hors champ (m)", s->portee, 0.f, 50.f);
			ec.Checkbox("boucle", s->boucle);
			ec.Checkbox("au demarrage", s->auDemarrage);
			ec.Checkbox("spatial", s->spatial);
		}

		void NkPanneauInspecteur::Animation(NkEditorFrameContext &ec, ecs::NkEntityId id) {
			bool ouvert = false;
			if (!EnTete(ec, id, NkComposantEditeur::NK_ANIMATION, ouvert) || !ouvert) {
				return;
			}
			NkAnimSprite2D *a = mM.scene.Monde().Get<NkAnimSprite2D>(id);
			int32 col = a->colonnes, lig = a->lignes;
			if (nkgui::InputInt(ec.Ui(), "colonnes de l'atlas", col)) {
				a->colonnes = static_cast<uint16>(col < 1 ? 1 : col);
			}
			if (nkgui::InputInt(ec.Ui(), "lignes de l'atlas", lig)) {
				a->lignes = static_cast<uint16>(lig < 1 ? 1 : lig);
			}
			if (a->nbClips > 0) {
				NkClipSprite &c = a->clips[0];
				int32 premiere = c.premiere, nombre = c.nombre;
				if (nkgui::InputInt(ec.Ui(), "clip 0 : premiere image", premiere)) {
					c.premiere = static_cast<uint16>(premiere < 0 ? 0 : premiere);
				}
				if (nkgui::InputInt(ec.Ui(), "clip 0 : images", nombre)) {
					c.nombre = static_cast<uint16>(nombre < 1 ? 1 : nombre);
				}
				ec.SliderFloat("images / s", c.imagesParSeconde, 0.f, 60.f);
			}
			ec.Checkbox("en pause", a->enPause);
		}

		void NkPanneauInspecteur::Ajouter(NkEditorFrameContext &ec, ecs::NkEntityId id) {
			auto &ctx = ec.Ui();
			ec.Separator();
			// La liste ne propose que ce qui PEUT s'ajouter : un composant deja
			// present, ou un corps rigide sur de la matiere, n'y figure pas.
			int32 n = 0;
			for (int32 k = 0; k < static_cast<int32>(NkComposantEditeur::NK_COUNT); ++k) {
				n += NkEditeurPeutAjouter(mM, id, static_cast<NkComposantEditeur>(k)) ? 1 : 0;
			}
			if (n == 0) {
				ec.Text("(tous les composants possibles sont la)");
				return;
			}
			// ⚠️ UNE LISTE EN LIGNE, PAS UNE LISTE DEROULANTE. La premiere version
			// passait par BeginCombo, puis par BeginPopup : sous Xvfb, AUCUN element
			// d'un popup de panneau ancre ne recevait le clic — ni le mien, ni la
			// liste « mode » du panneau Monde, qui est celle du kit (mesure du
			// 2026-09-29, a signaler a NKGui). Une liste EN LIGNE sous le bouton
			// n'a pas de couche a franchir : elle marche partout.
			if (ec.Button(mListeAjout ? "- Ajouter un composant" : "+ Ajouter un composant")) {
				mListeAjout = !mListeAjout;
			}
			if (mListeAjout) {
				for (int32 k = 0; k < static_cast<int32>(NkComposantEditeur::NK_COUNT); ++k) {
					const NkComposantEditeur c = static_cast<NkComposantEditeur>(k);
					if (!NkEditeurPeutAjouter(mM, id, c)) {
						continue;
					}
					if (c == NkComposantEditeur::NK_CORPS_MOU) {
						// Un corps mou, c'est une MATIERE : on la choisit ici.
						for (int32 i = 0; i < static_cast<int32>(NkActeurSim::NK_COUNT); ++i) {
							const NkActeurSim a = static_cast<NkActeurSim>(i);
							if (NkActeurSimInfo(a).rigide) {
								continue;
							}
							const NkString libelle = NkString::Format("Corps mou : %s", NkActeurSimInfo(a).nom);
							if (nkgui::Selectable(ctx, libelle.Data(), false)) {
								NkEditeurAjouterComposant(mM, id, c, a);
								mListeAjout = false;
							}
						}
						continue;
					}
					if (nkgui::Selectable(ctx, NkComposantEditeurNom(c), false)) {
						NkEditeurAjouterComposant(mM, id, c);
						mListeAjout = false;
					}
				}
			}
		}

		// =====================================================================
		// LE MONDE — les proprietes de la scene
		// =====================================================================
		void NkPanneauMonde::OnUI(NkEditorFrameContext &ec) {
			physics::NkParticules2D *p = mM.scene.Particules();
			ec.Text("Proprietes de la scene");
			ec.Separator();
			if (p != nullptr) {
				physics::NkReglagesP2D &r = p->reglages;
				// La gravite est UNE : celle des rigides suit celle des particules.
				float32 g = r.gravite.y;
				if (ec.SliderFloat("gravite (m/s2)", g, -30.f, 10.f)) {
					r.gravite.y = g;
					if (mM.scene.MondePhysique() != nullptr) {
						mM.scene.MondePhysique()->SetGravity(math::NkVec3f(r.gravite.x, g, 0.f));
					}
				}
				ec.SliderFloat("temperature", r.temperature, 0.f, 100.f);
				ec.SliderFloat("vent (m/s2)", r.vent, -20.f, 20.f);
				ec.SliderFloat("frein de l'air (1/s)", r.amortAir, 0.f, 3.f);
				nkgui::InputInt(ec.Ui(), "sous-pas", r.sousPas);
				r.sousPas = r.sousPas < 1 ? 1 : (r.sousPas > 32 ? 32 : r.sousPas);
				ec.Checkbox("collisions de la matiere", r.collisions);
				ec.Checkbox("soudure (atomes, blobs)", r.soudure);
				ec.Checkbox("boite de la matiere", r.limites.actif);
				ec.Separator();
				Ligne(ec, "matiere", NkString::Format("%u particules, %u liens, %u ruptures", static_cast<unsigned>(p->particules.Size()),
													 static_cast<unsigned>(p->LiensActifs()), static_cast<unsigned>(p->rupturesTotal)));
			} else {
				ec.Text("(cette scene n'a pas de matiere)");
			}
			Ligne(ec, "pas fixe", NkString::Format("%.4f s (%d max par trame)", static_cast<double>(mM.scene.Config().pasFixe),
												   mM.scene.Config().pasMaxParTrame));
			ec.Separator();
			ec.Text("Affichage de la matiere");
			static const char *kModes[4] = {"Eclaire", "Filaire", "Contraintes", "Vitesse"};
			int32 mode = static_cast<int32>(mM.rendu.mode);
			if (Choix(ec, "mode", kModes, 4, mode)) {
				mM.rendu.mode = static_cast<NkModeRenduParticules>(mode);
			}
			ec.Checkbox("liens", mM.rendu.liens);
			ec.Checkbox("particules", mM.rendu.particules);
			ec.Checkbox("vitesses", mM.rendu.vitesses);
		}

		// =====================================================================
		// OUTILS & APPAREIL
		// =====================================================================
		void NkPanneauOutils::OnUI(NkEditorFrameContext &ec) {
			ec.Text("Outil");
			static const char *kNoms[5] = {"Selection", "Poser", "Effacer", "Saisir", "Couteau"};
			for (int32 i = 0; i < 5; ++i) {
				if (nkgui::Selectable(ec.Ui(), kNoms[i], static_cast<int32>(mM.outil) == i)) {
					mM.outil = static_cast<NkOutil>(i);
				}
			}
			if (mM.outil == NkOutil::NK_SAISIR || mM.outil == NkOutil::NK_COUTEAU) {
				ec.Text("(agit sur la matiere : a utiliser en jeu)");
			}
			ec.Separator();
			ec.Text("Affichage");
			ec.Checkbox("grille", mM.voirGrille);
			ec.Checkbox("collisionneurs", mM.voirCollisionneurs);
			ec.Separator();
			// ── L'appareil simule ────────────────────────────────────────────
			const NkProfilAppareil p = mM.ProfilCourant();
			ec.Text("Appareil simule");
			ec.Text(NkString::Format("%s  %ux%u", p.nom, p.largeur, p.hauteur).Data());
			if (ec.Button("Appareil suivant")) {
				mM.profil = (mM.profil + 1) % NkNbProfils();
			}
			ec.Checkbox("paysage", mM.paysage);
			ec.Text(NkString::Format("zone sure  h:%.0f b:%.0f g:%.0f d:%.0f", p.zoneSure.top, p.zoneSure.bottom, p.zoneSure.left,
									 p.zoneSure.right)
						.Data());
			ec.Separator();
			// ⚠️ VUES et DESSINEES : l'ecart entre les deux EST la mesure du
			// hors-champ.
			ec.Text(NkString::Format("sprites vus %d / dessines %d", mM.stats.entitesVues, mM.stats.entitesDessinees).Data());
		}

	} // namespace editeur
} // namespace nkentseu
