// -----------------------------------------------------------------------------
// @File    NogeeBanc.cpp
// @Brief   LE BANC DE L'EDITEUR NOGEE (`Nogee --selftest`) : les actions du
//          modele, sans fenetre ni GPU, chacune avec sa contre-epreuve.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// La regle d'UnkenyEditor (NkEditeurActions.h) : « toute action de l'editeur est
// une fonction du modele, eprouvee par --selftest, jamais ecrite dans un dessin
// de panneau ». Le modele de Nogee (NogeeModele) tourne ici sur un monde ECS
// NU -- pas de NkEngineLayer, pas de renderer : ce que le banc prouve ne depend
// pas de l'ecran.
// -----------------------------------------------------------------------------

#include "Nogee/Editeur/NogeeModele.h"

#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Physics/NkPhysics.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "NKECS/Hierarchy/NkHierarchy.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nogee {

		using namespace ecs;
		using namespace math;

		namespace {
			int32 gReussis = 0;
			int32 gEchecs = 0;

			void Temoin(bool ok, const char *quoi) {
				(ok ? gReussis : gEchecs)++;
				std::printf("  [%s] %s\n", ok ? "OK" : "ECHEC", quoi);
			}

			NkEntityId Trouver(NkWorld &w, const char *nom) {
				NkEntityId r = NkEntityId::Invalid();
				w.Query<const NkName>().ForEach([&](NkEntityId id, const NkName &n) {
					if (!r.IsValid() && std::strcmp(n.value, nom) == 0) {
						r = id;
					}
				});
				return r;
			}

			int32 Compter(NogeeModele &m) {
				NkVector<NkEntityId> ids;
				m.Entites(ids);
				return static_cast<int32>(ids.Size());
			}

			int32 IndiceDe(NogeeGenre g) {
				int32 n = 0;
				const NogeeElement *cat = NogeeCatalogue(n);
				for (int32 k = 0; k < n; ++k) {
					if (cat[k].genre == g) {
						return k;
					}
				}
				return -1;
			}
		} // namespace

		int32 NogeeLancerBanc() {
			gReussis = 0;
			gEchecs = 0;
			std::printf("\nNogee — banc de l'éditeur (le modèle, sans fenêtre)\n");
			static NkWorld monde;
			NogeeModele m;
			m.Brancher(&monde, nullptr);

			// (n1) la scene de depart : celle de NogeDemo, la camera d'edition a part.
			m.NouvelleScene();
			Temoin(Compter(m) == 5, "(n1) nouvelle scène : 5 entités (sol, joueur, balle, caméra, soleil)");
			Temoin(m.Monde().IsAlive(m.cameraEditeur) && m.EstEditeur(m.cameraEditeur),
				   "(n1b) la caméra d'édition existe et n'est PAS dans l'Outliner");
			const NkEntityId joueur = Trouver(monde, "Joueur");
			Temoin(joueur.IsValid() && monde.Has<NkMeshComponent>(joueur) && monde.Has<NkRigidbody3D>(joueur),
				   "(n1c) le joueur : maillage + corps rigide");

			// (n2) Placer des acteurs : chaque genre pose ce que Noge sait rendre.
			const NkEntityId cube = m.PoserElement(IndiceDe(NogeeGenre::Cube), NkVec3f{2.f, 0.5f, 0.f});
			const NkMeshComponent *mc = monde.Get<NkMeshComponent>(cube);
			Temoin(mc != nullptr && mc->meshPath == NkString("primitive:cube") && m.selection == cube,
				   "(n2) Placer « Cube » : primitive:cube, choisi");
			const NkEntityId lampe = m.PoserElement(IndiceDe(NogeeGenre::LumierePonctuelle), NkVec3f{0.f, 3.f, 0.f});
			const NkLightComponent *lc = monde.Get<NkLightComponent>(lampe);
			Temoin(lc != nullptr && lc->type == NkLightType::Point, "(n2b) Placer « Lumière ponctuelle » : NkLightComponent Point");
			const NkEntityId cam = m.PoserElement(IndiceDe(NogeeGenre::Camera), NkVec3f{0.f, 2.f, 5.f});
			Temoin(monde.Has<NkCameraComponent>(cam), "(n2c) Placer « Caméra » : NkCameraComponent");
			Temoin(Compter(m) == 8, "(n2d) 8 entités après trois poses");

			// (n3) annuler / refaire : l'instantane rend le compte d'avant.
			Temoin(m.Annuler() && Compter(m) == 7, "(n3) Ctrl+Z : la caméra posée disparaît (7)");
			Temoin(m.Refaire() && Compter(m) == 8, "(n3b) Ctrl+Y : elle revient (8)");

			// (n4) dupliquer, renommer, supprimer.
			const NkEntityId cubeRef = Trouver(monde, "Cube");
			const NkEntityId copie = m.Dupliquer(cubeRef);
			Temoin(copie.IsValid() && Compter(m) == 9 && std::strcmp(m.Nom(copie), "Cube") != 0,
				   "(n4) Dupliquer : une entité de plus, un nom libre");
			m.Renommer(copie, "Copie du cube");
			Temoin(Trouver(monde, "Copie du cube").IsValid(), "(n4b) Renommer");
			m.Supprimer(copie);
			Temoin(Compter(m) == 8 && !Trouver(monde, "Copie du cube").IsValid(), "(n4c) Supprimer");

			// (n5) rattacher : l'enfant ne bouge pas dans le monde, son parent est pose.
			const NkEntityId sol = Trouver(monde, "Sol");
			const NkEntityId c2 = Trouver(monde, "Cube");
			m.Rattacher(c2, sol);
			Temoin(m.Parent(c2) == sol, "(n5) Rattacher le cube au sol");
			m.Rattacher(sol, c2);
			Temoin(m.Parent(sol) != c2, "(n5b) contre-épreuve : un parent ne descend pas de son enfant (refusé)");
			m.Rattacher(c2, NkEntityId::Invalid());
			Temoin(!m.Parent(c2).IsValid(), "(n5c) Détacher");

			// (n6) l'actif, l'oeil, le cadenas.
			m.Activer(c2, false);
			Temoin(!m.EstActive(c2) && monde.Has<NkInactive>(c2), "(n6) la case « active » éteint (NkInactive)");
			m.Activer(c2, true);
			Temoin(m.EstActive(c2), "(n6b) et rallume");
			m.BasculerCache(c2);
			Temoin(m.EstCache(c2), "(n6c) l'œil cache le maillage");
			m.BasculerCache(c2);
			m.BasculerVerrou(c2);
			Temoin(m.EstVerrouille(c2), "(n6d) le cadenas verrouille");
			m.BasculerVerrou(c2);

			// (n7) jouer / arreter : l'arret rend la scene d'avant le jeu.
			NkTransform *tj = monde.Get<NkTransform>(Trouver(monde, "Joueur"));
			const float32 yAvant = tj != nullptr ? tj->localPosition.y : 0.f;
			m.Jouer();
			Temoin(m.etat == NogeeEtatJeu::Jeu && m.PhysiqueAvance(), "(n7) Jouer : la physique avance");
			if (NkTransform *t = monde.Get<NkTransform>(Trouver(monde, "Joueur"))) {
				t->localPosition.y = 42.f; // ce que le jeu aurait fait
			}
			m.Arreter();
			const NkTransform *tApres = monde.Get<NkTransform>(Trouver(monde, "Joueur"));
			Temoin(m.etat == NogeeEtatJeu::Edition && tApres != nullptr && tApres->localPosition.y == yAvant,
				   "(n7b) Arrêter : le joueur revient à sa pose d'édition");
			Temoin(!m.PhysiqueAvance(), "(n7c) contre-épreuve : en édition, la physique n'avance pas");

			// (n8) enregistrer puis rouvrir : le meme compte, les memes noms.
			m.chemin = "Build/Nogee/banc_selftest.nkscene";
			const int32 avant = Compter(m);
			const bool ecrit = m.Enregistrer();
			m.NouvelleScene();
			const bool relu = m.Ouvrir("Build/Nogee/banc_selftest.nkscene");
			Temoin(ecrit && relu && Compter(m) == avant && Trouver(monde, "Cube").IsValid(),
				   "(n8) Enregistrer puis Ouvrir : la scène revient entière");
			int32 camerasEditeur = 0;
			monde.Query<const NkName>().ForEach([&](NkEntityId, const NkName &n) {
				camerasEditeur += std::strcmp(n.value, NogeeModele::kNomCameraEditeur) == 0 ? 1 : 0;
			});
			Temoin(m.Monde().IsAlive(m.cameraEditeur) && camerasEditeur == 1,
				   "(n8b) une seule caméra d'édition : elle n'est pas sauvée avec la scène, elle est refaite");
			(void)NkFile::Delete("Build/Nogee/banc_selftest.nkscene");

			std::printf("\n%s : %d reussis, %d echec%s\n", gEchecs == 0 ? "BANC NOGEE REUSSI" : "BANC NOGEE EN ECHEC", gReussis,
						gEchecs, gEchecs > 1 ? "s" : "");
			std::fflush(stdout);
			m.Vider();
			return gEchecs == 0 ? 0 : 1;
		}

	} // namespace nogee
} // namespace nkentseu
