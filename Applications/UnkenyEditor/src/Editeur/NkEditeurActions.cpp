// =============================================================================
// NkEditeurActions.cpp
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurViseur.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"

#include <cstdio>

namespace nkentseu {
	namespace editeur {

		namespace {
			void Aligner(NkEditeurModele &m) {
				// La photo et le fichier rendent les reglages des PARTICULES ; la
				// gravite des rigides les suit (elle doit rester UNE).
				if (m.scene.MondePhysique() != nullptr && m.scene.Particules() != nullptr) {
					const NkVec2f g = m.scene.Particules()->reglages.gravite;
					m.scene.MondePhysique()->SetGravity(math::NkVec3f(g.x, g.y, 0.f));
				}
			}

			ecs::NkEntityId Statique(NkScene &s, const char *nom, const NkVec2f &c, const NkVec2f &demi, uint32 couleur) {
				const ecs::NkEntityId e = s.Creer(nom, c);
				NkSprite2D sp;
				sp.taille = NkVec2f(demi.x * 2.f, demi.y * 2.f);
				sp.couleur = couleur;
				sp.couche = -10;
				sp.visible = false; // dessine par sa FORME (NkDessinerFormes) ; le sprite garde la couleur
				s.Monde().Add<NkSprite2D>(e, sp);
				NkCollisionneur2D col;
				col.demiTaille = demi;
				s.Monde().Add<NkCollisionneur2D>(e, col);
				NkCorps2D b;
				b.type = NkTypeCorps::NK_STATIQUE;
				b.friction = 0.6f;
				s.AjouterCorps(e, b);
				return e;
			}
		} // namespace

		// =====================================================================
		void NkEditeurNouvelleScene(NkEditeurModele &m) {
			NkSceneConfig cfg;
			cfg.physique = true;   // l'editeur exerce la physique : c'est son role
			cfg.particules = true; // et la matiere : corps mous, fluides, atomes
			cfg.gravite = NkVec2f(0.f, -9.81f);
			m.scene.Init(cfg);
			NkRemettreNomsSim();
			m.etat = NkEtatJeu::NK_EDITION;
			m.photo.valide = false;
			m.aSelection = false;
			m.pinceau = ecs::NkEntityId::Invalid();

			NkScene &s = m.scene;
			Statique(s, "Sol", NkVec2f(0.f, -4.5f), NkVec2f(11.f, 0.5f), 0x3E4756FFu);
			Statique(s, "Mur gauche", NkVec2f(-11.5f, 0.f), NkVec2f(0.5f, 5.f), 0x3E4756FFu);
			Statique(s, "Mur droit", NkVec2f(11.5f, 0.f), NkVec2f(0.5f, 5.f), 0x3E4756FFu);
			// Quelques caisses : elles rendent la physique VISIBLE des l'ouverture.
			for (int32 i = 0; i < 3; ++i) {
				NkPoserActeurSim(s, NkActeurSim::NK_CAISSE, NkVec2f(-6.f + static_cast<float32>(i) * 0.8f, -3.6f + static_cast<float32>(i) * 0.7f),
								 &m.ressources);
			}
			NkPoserActeurSim(s, NkActeurSim::NK_BLOB, NkVec2f(-1.5f, -1.f), &m.ressources);
			NkPoserActeurSim(s, NkActeurSim::NK_GELEE, NkVec2f(1.5f, -2.5f), &m.ressources);
			NkPoserActeurSim(s, NkActeurSim::NK_BALLON, NkVec2f(4.5f, -2.f), &m.ressources);
			NkPoserActeurSim(s, NkActeurSim::NK_EAU, NkVec2f(7.5f, -3.f), &m.ressources);
			NkPoserActeurSim(s, NkActeurSim::NK_TISSU, NkVec2f(-8.5f, 2.f), &m.ressources, 4.f);
		}

		// =====================================================================
		void NkEditeurJouer(NkEditeurModele &m) {
			if (!m.photo.valide) {
				m.scene.Photographier(m.photo); // ce que « Arreter » rendra
			}
			m.etat = NkEtatJeu::NK_JEU;
		}

		void NkEditeurPause(NkEditeurModele &m) {
			if (m.etat == NkEtatJeu::NK_JEU) {
				m.etat = NkEtatJeu::NK_PAUSE;
			}
		}

		void NkEditeurArreter(NkEditeurModele &m) {
			if (m.photo.valide) {
				m.scene.Restaurer(m.photo);
				Aligner(m);
			}
			m.photo.valide = false;
			m.etat = NkEtatJeu::NK_EDITION;
			// Les identifiants d'entite ont change (Restaurer) : une selection
			// retenue designerait une entite morte.
			m.aSelection = false;
			m.deplace = false;
			m.pinceau = ecs::NkEntityId::Invalid();
		}

		void NkEditeurUnPas(NkEditeurModele &m) {
			if (!m.photo.valide) {
				m.scene.Photographier(m.photo);
			}
			m.etat = NkEtatJeu::NK_PAUSE;
			m.scene.Pas(m.scene.Config().pasFixe);
		}

		void NkEditeurAvancer(NkEditeurModele &m, float32 dt) {
			m.messageAge += dt;
			if (m.etat == NkEtatJeu::NK_JEU && dt > 0.f) {
				m.scene.Pas(dt < 0.05f ? dt : 0.05f);
			}
			// Une selection dont l'entite a disparu (matiere gommee, tombee) : oubliee.
			if (m.aSelection && !m.scene.Monde().IsAlive(m.selection)) {
				m.aSelection = false;
				m.deplace = false;
			}
		}

		// =====================================================================
		ecs::NkEntityId NkEditeurPoser(NkEditeurModele &m, const NkVec2f &monde) {
			ecs::NkEntityId e;
			if (m.acteurSimple) {
				// L'entite elementaire d'origine de l'editeur : sprite + boite.
				m.graine = m.graine * 1664525u + 1013904223u;
				const uint32 teinte = 0x40404000u | ((m.graine >> 8) & 0x00BFBFBFu) | 0xFFu;
				e = m.scene.Creer("Entite", monde);
				NkSprite2D s;
				s.taille = NkVec2f(0.8f, 0.8f);
				s.couleur = teinte;
				m.scene.Monde().Add<NkSprite2D>(e, s);
				NkCollisionneur2D c;
				c.demiTaille = NkVec2f(0.4f, 0.4f);
				m.scene.Monde().Add<NkCollisionneur2D>(e, c);
				NkCorps2D b;
				m.scene.AjouterCorps(e, b);
			} else {
				e = NkPoserActeurSim(m.scene, m.acteur, monde, &m.ressources);
			}
			if (e.IsValid()) {
				m.selection = e;
				m.aSelection = true;
			}
			return e;
		}

		bool NkEditeurCentreSelection(const NkEditeurModele &m, NkVec2f &centre) {
			if (!m.aSelection) {
				return false;
			}
			NkScene &s = const_cast<NkScene &>(m.scene);
			if (const NkCorpsMou2D *mou = s.Monde().Get<NkCorpsMou2D>(m.selection)) {
				const physics::NkParticules2D *p = s.Particules();
				const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
				if (ci >= 0) {
					centre = p->CentreCorps(static_cast<uint32>(ci));
					return true;
				}
			}
			if (const NkTransform2D *t = s.Monde().Get<NkTransform2D>(m.selection)) {
				centre = t->position;
				return true;
			}
			return false;
		}

		bool NkEditeurChoisirSous(NkEditeurModele &m, const NkVec2f &monde, NkVec2f *centre) {
			NkScene &s = m.scene;
			// 1. La matiere : c'est elle qui est dessinee PAR-DESSUS le decor.
			if (physics::NkParticules2D *p = s.Particules()) {
				const int32 i = p->ParticuleProche(monde, 0.25f);
				if (i >= 0) {
					const uint32 id = p->corps[p->particules[static_cast<uint32>(i)].corps].id;
					const ecs::NkEntityId e = s.EntiteDuCorpsMou(id);
					if (e.IsValid()) {
						m.selection = e;
						m.aSelection = true;
						if (centre != nullptr) {
							NkEditeurCentreSelection(m, *centre);
						}
						return true;
					}
				}
			}
			// 2. Les sprites visibles (le plus haut dessine gagne).
			ecs::NkEntityId trouve;
			NkVec2f c;
			if (NkEntiteSous(s, monde, trouve, c)) {
				m.selection = trouve;
				m.aSelection = true;
				if (centre != nullptr) {
					*centre = c;
				}
				return true;
			}
			// 3. Les formes sans sprite visible : balles, obstacles, sol.
			float32 meilleur = 0.1f;
			bool aucun = true;
			s.Monde().Query<NkTransform2D, NkCollisionneur2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t, NkCollisionneur2D &col) {
				const float32 d = NkDistanceForme2D(t, col, monde);
				if (d < meilleur) {
					meilleur = d;
					trouve = id;
					c = t.position;
					aucun = false;
				}
			});
			if (!aucun) {
				m.selection = trouve;
				m.aSelection = true;
				if (centre != nullptr) {
					*centre = c;
				}
				return true;
			}
			m.aSelection = false;
			return false;
		}

		void NkEditeurDeplacer(NkEditeurModele &m, const NkVec2f &cible) {
			if (!m.aSelection) {
				return;
			}
			NkScene &s = m.scene;
			if (const NkCorpsMou2D *mou = s.Monde().Get<NkCorpsMou2D>(m.selection)) {
				physics::NkParticules2D *p = s.Particules();
				const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
				if (ci >= 0) {
					const NkVec2f c = p->CentreCorps(static_cast<uint32>(ci));
					p->Translater(static_cast<uint32>(ci), cible - c);
					// Le transform suit tout de suite (sans attendre un Pas).
					if (NkTransform2D *t = s.Monde().Get<NkTransform2D>(m.selection)) {
						t->position = cible;
					}
				}
				return;
			}
			// ⚠️ TELEPORTER, pas ecrire le transform : le solveur garderait
			// l'ancienne position et l'objet reviendrait d'un coup au pas suivant.
			s.TeleporterEntite(m.selection, cible);
		}

		bool NkEditeurBoiteSelection(NkEditeurModele &m, NkVec2f &mn, NkVec2f &mx) {
			if (!m.aSelection) {
				return false;
			}
			NkScene &s = m.scene;
			if (const NkCorpsMou2D *mou = s.Monde().Get<NkCorpsMou2D>(m.selection)) {
				const physics::NkParticules2D *p = s.Particules();
				const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
				if (ci >= 0) {
					p->BoiteCorps(static_cast<uint32>(ci), mn, mx);
					return true;
				}
				return false;
			}
			const NkTransform2D *t = s.Monde().Get<NkTransform2D>(m.selection);
			if (t == nullptr) {
				return false;
			}
			float32 hw = 0.5f, hh = 0.5f;
			if (const NkCollisionneur2D *c = s.Monde().Get<NkCollisionneur2D>(m.selection)) {
				hw = c->forme == NkForme2D::NK_CERCLE ? c->rayon : c->demiTaille.x + (c->forme == NkForme2D::NK_CAPSULE ? c->rayon : 0.f);
				hh = c->forme == NkForme2D::NK_BOITE ? c->demiTaille.y : c->rayon;
				// Une forme TOURNEE : la boite englobante couvre sa diagonale.
				if (t->rotation != 0.f) {
					const float32 r = math::NkSqrt(hw * hw + hh * hh);
					hw = hh = r;
				}
			} else if (const NkSprite2D *sp = s.Monde().Get<NkSprite2D>(m.selection)) {
				hw = sp->taille.x * 0.5f;
				hh = sp->taille.y * 0.5f;
			}
			mn = NkVec2f(t->position.x - hw, t->position.y - hh);
			mx = NkVec2f(t->position.x + hw, t->position.y + hh);
			return true;
		}

		void NkEditeurSupprimerSelection(NkEditeurModele &m) {
			if (!m.aSelection) {
				return;
			}
			m.scene.Detruire(m.selection);
			m.aSelection = false;
			m.deplace = false;
		}

		bool NkEditeurEffacerSous(NkEditeurModele &m, const NkVec2f &monde) {
			const bool avait = m.aSelection;
			const ecs::NkEntityId avant = m.selection;
			if (!NkEditeurChoisirSous(m, monde)) {
				m.aSelection = avait;
				m.selection = avant;
				return false;
			}
			NkEditeurSupprimerSelection(m);
			return true;
		}

		// =====================================================================
		// Entites et composants
		// =====================================================================
		ecs::NkEntityId NkEditeurCreerEntite(NkEditeurModele &m, const char *nom, const NkVec2f &position) {
			const ecs::NkEntityId e = m.scene.Creer(nom != nullptr && nom[0] != '\0' ? nom : "Entite", position);
			m.selection = e;
			m.aSelection = true;
			return e;
		}

		bool NkEditeurRenommer(NkEditeurModele &m, ecs::NkEntityId id, const char *nom) {
			if (!m.scene.Monde().IsAlive(id) || nom == nullptr) {
				return false;
			}
			NkEtiquette e;
			std::snprintf(e.nom, sizeof(e.nom), "%s", nom);
			m.scene.Monde().Set<NkEtiquette>(id, e);
			return true;
		}

		ecs::NkEntityId NkEditeurDupliquer(NkEditeurModele &m) {
			if (!m.aSelection) {
				return ecs::NkEntityId::Invalid();
			}
			ecs::NkWorld &w = m.scene.Monde();
			const ecs::NkEntityId src = m.selection;
			if (w.Has<NkCorpsMou2D>(src)) {
				NkEditeurAnnoncer(m, "La matiere d'un corps mou ne se duplique pas : posez-en un autre");
				return ecs::NkEntityId::Invalid();
			}
			const NkTransform2D *t = w.Get<NkTransform2D>(src);
			if (t == nullptr) {
				return ecs::NkEntityId::Invalid();
			}
			char nom[32] = "Copie";
			if (const NkEtiquette *et = w.Get<NkEtiquette>(src)) {
				std::snprintf(nom, sizeof(nom), "%.26s copie", et->nom);
			}
			NkTransform2D tt = *t;
			tt.position = tt.position + NkVec2f(0.5f, 0.5f);
			const ecs::NkEntityId e = m.scene.Creer(nom, tt.position);
			w.Set<NkTransform2D>(e, tt);
			if (const NkSprite2D *s = w.Get<NkSprite2D>(src)) {
				NkSprite2D c = *s;
				w.Add<NkSprite2D>(e, c);
			}
			if (const NkCollisionneur2D *s = w.Get<NkCollisionneur2D>(src)) {
				NkCollisionneur2D c = *s;
				w.Add<NkCollisionneur2D>(e, c);
			}
			if (const NkAnimSprite2D *s = w.Get<NkAnimSprite2D>(src)) {
				NkAnimSprite2D c = *s;
				w.Add<NkAnimSprite2D>(e, c);
			}
			if (const NkSource2D *s = w.Get<NkSource2D>(src)) {
				NkSource2D c = *s;
				c.voix = 0u;
				c.lance = false;
				w.Add<NkSource2D>(e, c);
			}
			if (const NkCorps2D *s = w.Get<NkCorps2D>(src)) {
				NkCorps2D c = *s;
				c.corpsId = 0;
				m.scene.AjouterCorps(e, c);
			}
			m.selection = e;
			m.aSelection = true;
			return e;
		}

		const char *NkComposantEditeurNom(NkComposantEditeur c) noexcept {
			switch (c) {
				case NkComposantEditeur::NK_SPRITE:
					return "Sprite";
				case NkComposantEditeur::NK_COLLISIONNEUR:
					return "Collisionneur";
				case NkComposantEditeur::NK_CORPS:
					return "Corps rigide";
				case NkComposantEditeur::NK_CORPS_MOU:
					return "Corps mou";
				case NkComposantEditeur::NK_SOURCE:
					return "Source sonore";
				case NkComposantEditeur::NK_ANIMATION:
					return "Animation";
				default:
					return "";
			}
		}

		bool NkEditeurAUnComposant(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c) {
			ecs::NkWorld &w = m.scene.Monde();
			switch (c) {
				case NkComposantEditeur::NK_SPRITE:
					return w.Has<NkSprite2D>(id);
				case NkComposantEditeur::NK_COLLISIONNEUR:
					return w.Has<NkCollisionneur2D>(id);
				case NkComposantEditeur::NK_CORPS:
					return w.Has<NkCorps2D>(id);
				case NkComposantEditeur::NK_CORPS_MOU:
					return w.Has<NkCorpsMou2D>(id);
				case NkComposantEditeur::NK_SOURCE:
					return w.Has<NkSource2D>(id);
				case NkComposantEditeur::NK_ANIMATION:
					return w.Has<NkAnimSprite2D>(id);
				default:
					return false;
			}
		}

		bool NkEditeurPeutAjouter(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c) {
			if (!m.scene.Monde().IsAlive(id) || NkEditeurAUnComposant(m, id, c)) {
				return false;
			}
			ecs::NkWorld &w = m.scene.Monde();
			switch (c) {
				case NkComposantEditeur::NK_CORPS:
					// Un corps rigide ET une matiere : deux solveurs qui se
					// disputeraient la meme position.
					return m.scene.PhysiqueActive() && !w.Has<NkCorpsMou2D>(id);
				case NkComposantEditeur::NK_CORPS_MOU:
					return m.scene.Particules() != nullptr && !w.Has<NkCorps2D>(id);
				default:
					return true;
			}
		}

		bool NkEditeurAjouterComposant(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c, NkActeurSim matiere) {
			if (!NkEditeurPeutAjouter(m, id, c)) {
				return false;
			}
			ecs::NkWorld &w = m.scene.Monde();
			switch (c) {
				case NkComposantEditeur::NK_SPRITE: {
					NkSprite2D s;
					s.couleur = 0xC8CCD8FFu;
					w.Add<NkSprite2D>(id, s);
					return true;
				}
				case NkComposantEditeur::NK_COLLISIONNEUR: {
					NkCollisionneur2D col;
					// A la taille du sprite, s'il y en a un : ce qu'on voit touche.
					if (const NkSprite2D *s = w.Get<NkSprite2D>(id)) {
						col.demiTaille = NkVec2f(s->taille.x * 0.5f, s->taille.y * 0.5f);
					}
					w.Add<NkCollisionneur2D>(id, col);
					return true;
				}
				case NkComposantEditeur::NK_CORPS: {
					if (!w.Has<NkCollisionneur2D>(id)) {
						NkEditeurAjouterComposant(m, id, NkComposantEditeur::NK_COLLISIONNEUR);
					}
					NkCorps2D b;
					return m.scene.AjouterCorps(id, b);
				}
				case NkComposantEditeur::NK_CORPS_MOU: {
					if (NkActeurSimInfo(matiere).rigide) {
						return false;
					}
					const NkTransform2D *t = w.Get<NkTransform2D>(id);
					const int32 ci = NkCreerMatiereSim(m.scene, matiere, t != nullptr ? t->position : NkVec2f(0.f, 0.f));
					return m.scene.AttacherCorpsMou(id, ci, NkActeurSimInfo(matiere).couleur);
				}
				case NkComposantEditeur::NK_SOURCE: {
					NkSource2D s;
					w.Add<NkSource2D>(id, s);
					return true;
				}
				case NkComposantEditeur::NK_ANIMATION: {
					NkAnimSprite2D a;
					a.nbClips = 1;
					w.Add<NkAnimSprite2D>(id, a);
					return true;
				}
				default:
					return false;
			}
		}

		bool NkEditeurRetirerComposant(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c) {
			if (!NkEditeurAUnComposant(m, id, c)) {
				return false;
			}
			ecs::NkWorld &w = m.scene.Monde();
			switch (c) {
				case NkComposantEditeur::NK_SPRITE:
					w.Remove<NkSprite2D>(id);
					return true;
				case NkComposantEditeur::NK_COLLISIONNEUR:
					// Le corps rigide s'appuie sur sa forme : il part avec elle.
					m.scene.RetirerCorps(id);
					w.Remove<NkCollisionneur2D>(id);
					return true;
				case NkComposantEditeur::NK_CORPS:
					return m.scene.RetirerCorps(id);
				case NkComposantEditeur::NK_CORPS_MOU:
					return m.scene.RetirerCorpsMou(id);
				case NkComposantEditeur::NK_SOURCE:
					w.Remove<NkSource2D>(id);
					return true;
				case NkComposantEditeur::NK_ANIMATION:
					w.Remove<NkAnimSprite2D>(id);
					return true;
				default:
					return false;
			}
		}

		const char *NkEditeurTypeDe(NkScene &scene, ecs::NkEntityId id) {
			if (scene.Monde().Has<NkCorpsMou2D>(id)) {
				return "mou";
			}
			if (const NkCorps2D *c = scene.Monde().Get<NkCorps2D>(id)) {
				return c->type == NkTypeCorps::NK_DYNAMIQUE ? "rigide" : "decor";
			}
			return "entite";
		}

		// =====================================================================
		const char *NkEditeurChemin(NkEditeurModele &m) {
			if (m.chemin.Empty()) {
				const NkPath base = NkDirectory::GetAppDataDirectory();
				if (!base.ToString().Empty()) {
					const NkPath dossier = base / "UnkenyEditor";
					NkDirectory::CreateRecursive(dossier);
					m.chemin = (dossier / "scene.nkscene").ToString();
				} else {
					m.chemin = "scene.nkscene";
				}
			}
			return m.chemin.CStr();
		}

		void NkEditeurAnnoncer(NkEditeurModele &m, const char *texte) {
			m.message = texte;
			m.messageAge = 0.f;
		}

		bool NkEditeurSauver(NkEditeurModele &m) {
			// On enregistre ce qu'on EDITE : en jeu, la photo d'avant « Jouer »
			// serait perdue au profit d'un instant de simulation. On arrete d'abord.
			if (m.etat != NkEtatJeu::NK_EDITION) {
				NkEditeurArreter(m);
			}
			const bool ok = NkSauverSceneFichier(m.scene, NkEditeurChemin(m), &m.textures);
			NkEditeurAnnoncer(m, ok ? "Scene enregistree" : "Enregistrement impossible");
			return ok;
		}

		bool NkEditeurOuvrir(NkEditeurModele &m) {
			NkString erreur;
			const bool ok = NkChargerSceneFichier(m.scene, NkEditeurChemin(m), &m.textures, &erreur);
			if (ok) {
				Aligner(m);
				m.etat = NkEtatJeu::NK_EDITION;
				m.photo.valide = false;
				m.aSelection = false;
				NkEditeurAnnoncer(m, "Scene ouverte");
			} else {
				NkEditeurAnnoncer(m, erreur.Empty() ? "Aucune scene enregistree" : erreur.CStr());
			}
			return ok;
		}

	} // namespace editeur
} // namespace nkentseu
