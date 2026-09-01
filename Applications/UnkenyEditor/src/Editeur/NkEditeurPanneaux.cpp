// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurPanneaux.cpp
// DESCRIPTION: Les panneaux de l'editeur.
//
// ⚠️ LE PLACEMENT DES BOUTONS N'EXISTE QU'UNE FOIS
//   `NkRectOutil` et `NkRectBouton` sont appelees PAR LE DESSIN et PAR LE TEST
//   DE CLIC. Deux geometries paralleles — une pour dessiner, une pour tester —
//   sont le defaut que ce depot documente : elles s'accordent le premier jour,
//   et le jour ou la barre change, une seule suit. Le bouton devient alors
//   inerte, et on cherche le defaut dans le routage des entrees.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurPanneaux.h"
#include "Editeur/NkEditeurApp.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using math::NkVec2f;

		namespace {
			const float32 kMargeBarre = 8.f;
			const float32 kLargeurOutil = 92.f;
			const float32 kLargeurBouton = 116.f;

			/// LE placement des quatre outils. Une seule definition.
			NkRect RectOutil(const NkDispoEditeur &dispo, int32 i) noexcept {
				const float32 h = dispo.barre.h - kMargeBarre * 2.f;
				return NkRect{kMargeBarre + static_cast<float32>(i) * (kLargeurOutil + 4.f), dispo.barre.y + kMargeBarre,
							  kLargeurOutil, h};
			}

			/// LE placement des boutons de droite. `i` compte depuis la droite.
			NkRect RectBouton(const NkDispoEditeur &dispo, int32 i) noexcept {
				const float32 h = dispo.barre.h - kMargeBarre * 2.f;
				return NkRect{dispo.barre.x + dispo.barre.w - kMargeBarre -
								  static_cast<float32>(i + 1) * (kLargeurBouton + 4.f),
							  dispo.barre.y + kMargeBarre, kLargeurBouton, h};
			}

			const char *NomOutil(int32 i) noexcept {
				switch (i) {
					case 1: return "Poser";
					case 2: return "Peindre";
					case 3: return "Effacer";
					default: return "Selection";
				}
			}
		} // namespace

		// =====================================================================
		void NkDessinerBarre(nkgui::NkGuiDrawList &dl, const NkDispoEditeur &dispo, nkgui::NkGuiFont *police,
							 const NkTheme &th, NkOutil outil, const NkProfilAppareil &profil, bool paysage,
							 bool simule, bool collisionneurs, bool grille) {
			dl.AddRectFilled(dispo.barre, th.panneau);
			dl.AddLine(NkVec2f(0.f, dispo.barre.y + dispo.barre.h),
					   NkVec2f(dispo.barre.w, dispo.barre.y + dispo.barre.h), th.bord, 1.f);

			for (int32 i = 0; i < 4; ++i) {
				const bool actif = (static_cast<int32>(outil) == i);
				NkBouton(dl, RectOutil(dispo, i), police, NomOutil(i), th, actif);
			}

			// A droite, du bord vers l'interieur : profil, orientation, puis les
			// bascules d'affichage, puis les actions.
			const NkString nomProfil = NkString::Format("%s%s", profil.nom, paysage ? " (P)" : "");
			NkBouton(dl, RectBouton(dispo, 0), police, nomProfil.Data(), th, true);
			NkBouton(dl, RectBouton(dispo, 1), police, paysage ? "Portrait" : "Paysage", th, false);
			NkBouton(dl, RectBouton(dispo, 2), police, simule ? "Pause" : "Simuler", th, simule);
			NkBouton(dl, RectBouton(dispo, 3), police, collisionneurs ? "Formes ON" : "Formes OFF", th,
					 collisionneurs);
			NkBouton(dl, RectBouton(dispo, 4), police, grille ? "Grille ON" : "Grille OFF", th, grille);
			NkBouton(dl, RectBouton(dispo, 5), police, "Cadrer", th, false);
			NkBouton(dl, RectBouton(dispo, 6), police, "Supprimer", th, false);
		}

		NkActionBarre NkBarreClic(const NkDispoEditeur &dispo, const NkTheme &th, const NkVec2f &point, NkOutil &outil,
								  int32 &profil, bool &paysage, bool &simule, bool &collisionneurs, bool &grille) {
			(void)th;
			for (int32 i = 0; i < 4; ++i) {
				if (NkDansRect(RectOutil(dispo, i), point)) {
					outil = static_cast<NkOutil>(i);
					return NkActionBarre::NK_OUTIL_CHANGE;
				}
			}
			if (NkDansRect(RectBouton(dispo, 0), point)) {
				profil = (profil + 1) % NkNbProfils();
				return NkActionBarre::NK_PROFIL_CHANGE;
			}
			if (NkDansRect(RectBouton(dispo, 1), point)) {
				paysage = !paysage;
				return NkActionBarre::NK_PROFIL_CHANGE; // le rapport d'ecran change aussi
			}
			if (NkDansRect(RectBouton(dispo, 2), point)) {
				simule = !simule;
				return NkActionBarre::NK_SIMULATION_CHANGE;
			}
			if (NkDansRect(RectBouton(dispo, 3), point)) {
				collisionneurs = !collisionneurs;
				return NkActionBarre::NK_AFFICHAGE_CHANGE;
			}
			if (NkDansRect(RectBouton(dispo, 4), point)) {
				grille = !grille;
				return NkActionBarre::NK_AFFICHAGE_CHANGE;
			}
			if (NkDansRect(RectBouton(dispo, 5), point)) {
				return NkActionBarre::NK_CADRER;
			}
			if (NkDansRect(RectBouton(dispo, 6), point)) {
				return NkActionBarre::NK_SUPPRIMER;
			}
			return NkActionBarre::NK_RIEN;
		}

		// =====================================================================
		namespace {
			const float32 kLigneHierarchie = 24.f;

			/// LE placement d'une ligne de hierarchie. Appelee par le dessin ET
			/// par le test de clic — meme raison que pour la barre.
			NkRect RectLigne(const NkRect &zone, int32 i) noexcept {
				return NkRect{zone.x + 6.f, zone.y + 34.f + static_cast<float32>(i) * kLigneHierarchie, zone.w - 12.f,
							  kLigneHierarchie - 2.f};
			}
		} // namespace

		void NkDessinerHierarchie(nkgui::NkGuiDrawList &dl, const NkRect &zone, nkgui::NkGuiFont *police,
								  NkScene &scene, const NkTheme &th, const ecs::NkEntityId *selection) {
			dl.AddRectFilled(zone, th.panneau);
			renderer::NkTexte(dl, police, zone.x + 10.f, zone.y + 10.f, "Hierarchie", th.texte);

			int32 i = 0;
			scene.Monde().Query<NkTransform2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &) {
				const NkRect ligne = RectLigne(zone, i);
				if (ligne.y + ligne.h > zone.y + zone.h) {
					++i;
					return; // hors du panneau : on ne dessine pas par-dessus le voisin
				}
				// NkEntityId porte SON operator== : il compare l'index ET la
				// generation. Comparer le seul index confondrait une entite
				// detruite avec celle qui a repris sa place — c'est exactement
				// ce que la generation existe pour empecher.
				const bool choisie = (selection != nullptr && *selection == id);
				if (choisie) {
					dl.AddRectFilled(ligne, th.panneauActif, 4.f);
					dl.AddRect(ligne, th.accent, 1.5f, 4.f);
				}
				const NkEtiquette *e = scene.Monde().Get<NkEtiquette>(id);
				const NkString nom = (e != nullptr && e->nom[0] != '\0')
										 ? NkString(e->nom)
										 : NkString::Format("Entite %u", static_cast<uint32>(id.index));
				renderer::NkTexte(dl, police, ligne.x + 8.f, ligne.y + 4.f, nom.Data(),
								  choisie ? th.texte : th.texteFaible, ligne.w - 12.f);
				++i;
			});
		}

		bool NkHierarchieClic(const NkRect &zone, NkScene &scene, const NkVec2f &point, ecs::NkEntityId &choisi) {
			bool trouve = false;
			int32 i = 0;
			scene.Monde().Query<NkTransform2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &) {
				if (!trouve && NkDansRect(RectLigne(zone, i), point)) {
					choisi = id;
					trouve = true;
				}
				++i;
			});
			return trouve;
		}

		// =====================================================================
		void NkDessinerInspecteur(nkgui::NkGuiDrawList &dl, const NkRect &zone, nkgui::NkGuiFont *petite,
								  nkgui::NkGuiFont *corps, NkScene &scene, const NkTheme &th,
								  const ecs::NkEntityId *selection) {
			dl.AddRectFilled(zone, th.panneau);
			renderer::NkTexte(dl, petite, zone.x + 10.f, zone.y + 10.f, "Inspecteur", th.texte);

			if (selection == nullptr) {
				renderer::NkTexte(dl, petite, zone.x + 10.f, zone.y + 40.f, "Aucune selection", th.texteFaible,
								  zone.w - 20.f);
				return;
			}
			float32 y = zone.y + 38.f;
			auto ligne = [&](const char *cle, const NkString &valeur) {
				renderer::NkTexte(dl, petite, zone.x + 10.f, y, cle, th.texteFaible, zone.w * 0.45f);
				renderer::NkTexteADroite(dl, petite, zone.x + zone.w - 10.f, y, valeur.Data(), th.texte);
				y += 20.f;
			};
			auto titre = [&](const char *t) {
				y += 8.f;
				renderer::NkTexte(dl, corps, zone.x + 10.f, y, t, th.accent);
				y += 22.f;
			};

			if (const NkEtiquette *e = scene.Monde().Get<NkEtiquette>(*selection)) {
				ligne("nom", NkString(e->nom));
			}

			if (const NkTransform2D *t = scene.Monde().Get<NkTransform2D>(*selection)) {
				titre("Transform");
				ligne("x", NkString::Format("%.2f", t->position.x));
				ligne("y", NkString::Format("%.2f", t->position.y));
				ligne("rotation", NkString::Format("%.1f deg", t->rotation * 57.2957795f));
				ligne("echelle", NkString::Format("%.2f x %.2f", t->echelle.x, t->echelle.y));
			}

			if (const NkSprite2D *s = scene.Monde().Get<NkSprite2D>(*selection)) {
				titre("Sprite");
				ligne("taille", NkString::Format("%.2f x %.2f", s->taille.x, s->taille.y));
				ligne("couche", NkString::Format("%d", s->couche));
				ligne("visible", NkString(s->visible ? "oui" : "non"));
			}

			if (const NkCollisionneur2D *c = scene.Monde().Get<NkCollisionneur2D>(*selection)) {
				titre("Collisionneur");
				const char *f = (c->forme == NkForme2D::NK_CERCLE)
									? "cercle"
									: ((c->forme == NkForme2D::NK_CAPSULE) ? "capsule" : "boite");
				ligne("forme", NkString(f));
				ligne("demi-taille", NkString::Format("%.2f x %.2f", c->demiTaille.x, c->demiTaille.y));
				ligne("declencheur", NkString(c->declencheur ? "oui" : "non"));
			}

			if (const NkCorps2D *b = scene.Monde().Get<NkCorps2D>(*selection)) {
				titre("Corps");
				const char *ty = (b->type == NkTypeCorps::NK_STATIQUE)
									 ? "statique"
									 : ((b->type == NkTypeCorps::NK_CINEMATIQUE) ? "cinematique" : "dynamique");
				ligne("type", NkString(ty));
				// ⚠️ L'identifiant du corps est affiche : c'est LUI qui dit si
				// l'entite est reellement enregistree dans le solveur. Zero =
				// elle ne l'est pas, et aucune force ne l'atteindra jamais —
				// defaut qu'on cherche autrement pendant une heure.
				ligne("corps #", NkString::Format("%u", b->corpsId));
				const NkVec2f v = scene.Vitesse(*selection);
				ligne("vitesse", NkString::Format("%.2f, %.2f", v.x, v.y));
			}
		}

		// =====================================================================
		void NkDessinerEtat(nkgui::NkGuiDrawList &dl, const NkRect &zone, nkgui::NkGuiFont *police, const NkTheme &th,
							NkScene &scene, const NkStatsRendu &stats, const NkProfilAppareil &profil) {
			dl.AddRectFilled(zone, th.panneauActif);

			// ⚠️ Le pied de page affiche des MESURES, pas des libelles. « vues »
			// contre « dessinees » dit si le hors-champ fonctionne ; « pas »
			// dit si la physique rattrape ou si elle jette du retard. Sans ces
			// deux paires, les deux questions n'ont pas de reponse.
			const NkString texte = NkString::Format(
				"%s  %ux%u  densite %.1f   |   entites vues %d, dessinees %d   |   pas physique %d   |   %s",
				profil.nom, profil.largeur, profil.hauteur, profil.densite, stats.entitesVues, stats.entitesDessinees,
				scene.DernierNbPas(), scene.PhysiqueActive() ? "physique active" : "sans physique");
			renderer::NkTexte(dl, police, zone.x + 10.f, zone.y + 5.f, texte.Data(), th.texteFaible, zone.w - 20.f);
		}

	} // namespace editeur
} // namespace nkentseu
