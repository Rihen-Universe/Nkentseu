//
// NkEditeurBancLumiere.cpp
// =============================================================================
// Description :
//   `UnkenyEditor --selftest` : les gestes de l'editeur sur l'eclairage 2D et
//   les effets. Compte A PART (« BANC LUMIERE EDITEUR ») : les 21 temoins du
//   banc des actions gardent leur compte.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (x1)  la scene NEUVE n'a rien change : eclairage eteint, ni lumiere ni
//         emetteur — l'existant de l'editeur se dessine comme avant
//   (x2)  « + Ajouter » : Lumiere 2D et Emetteur s'ajoutent a une entite vide,
//         pas deux fois ; « Retirer » les enleve, l'entite reste
//   (x3)  poser une lumiere N'ALLUME PAS l'eclairage de la scene (facultatif),
//         et l'annonce le dit
//   (x4)  appliquer un preset change la recette et GARDE la graine
//   (x5)  la poignee de portee : 7,5 m posee ; 0,05 m ramene a 0,1 ; sur un
//         emetteur, c'est la portee de sa lumiere liee
//   (x6)  l'icone d'une lumiere se choisit au clic (la lanterne de la nuit)
//   (x7)  l'APERCU en edition fait bruler le feu sans bouger aucune caisse
//   (x8)  Jouer puis Arreter : ambiante et lumieres reviennent, les particules
//         repartent de zero, autant d'entites qu'avant
//   (x9)  Enregistrer puis Ouvrir la nuit : memes lumieres, memes emetteurs,
//         meme reglage d'eclairage
//   (x10) dupliquer un feu : la copie a son propre hasard (autre graine)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurLumiere.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			int32 gE = 0;
			int32 gR = 0;

			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			int32 Compter(NkScene &s, int32 quoi) {
				int32 n = 0;
				if (quoi == 0) {
					s.Monde().Query<NkLumiere2D>().ForEach([&n](ecs::NkEntityId, NkLumiere2D &) { ++n; });
				} else {
					s.Monde().Query<NkEmetteur2D>().ForEach([&n](ecs::NkEntityId, NkEmetteur2D &) { ++n; });
				}
				return n;
			}

			uint32 NbEntites(NkScene &s) {
				NkVector<ecs::NkEntityId> ids;
				s.Entites(ids);
				return static_cast<uint32>(ids.Size());
			}

			ecs::NkEntityId Par(NkScene &s, const char *nom) {
				ecs::NkEntityId t = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (std::strcmp(e.nom, nom) == 0 && !t.IsValid()) {
						t = id;
					}
				});
				return t;
			}
		} // namespace

		int32 NkEditeurLancerBancLumiere() {
			gE = 0;
			gR = 0;
			std::printf("\nUnkenyEditor — banc de l'eclairage et des effets\n\n");
			NkEditeurModele *pm = memory::NkGetDefaultAllocator().New<NkEditeurModele>(); // gros : sur le tas
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);

			// (x1)
			Temoin(!m.scene.Eclairage().actif && Compter(m.scene, 0) == 0 && Compter(m.scene, 1) == 0 &&
					   NkEclairageParDefaut(m.scene.Eclairage()),
				   "(x1) scene neuve : eclairage eteint, ni lumiere ni emetteur", 0.f);

			// (x2)
			const ecs::NkEntityId e = NkEditeurCreerEntite(m, "Vide", NkVec2f(0.f, 2.f));
			const bool peutL = NkEditeurPeutAjouter(m, e, NkComposantEditeur::NK_LUMIERE);
			const bool ajL = NkEditeurAjouterComposant(m, e, NkComposantEditeur::NK_LUMIERE);
			const bool ajL2 = NkEditeurAjouterComposant(m, e, NkComposantEditeur::NK_LUMIERE);
			const bool ajE = NkEditeurAjouterComposant(m, e, NkComposantEditeur::NK_EMETTEUR);
			const NkEmetteur2D *em = m.scene.Monde().Get<NkEmetteur2D>(e);
			const bool feu = em != nullptr && em->preset == NkPresetEffet2D::NK_FEU;
			const bool retL = NkEditeurRetirerComposant(m, e, NkComposantEditeur::NK_LUMIERE);
			const bool retE = NkEditeurRetirerComposant(m, e, NkComposantEditeur::NK_EMETTEUR);
			Temoin(peutL && ajL && !ajL2 && ajE && feu && retL && retE && m.scene.Monde().IsAlive(e) &&
					   !m.scene.Monde().Has<NkLumiere2D>(e) && !m.scene.Monde().Has<NkEmetteur2D>(e),
				   "(x2) + Ajouter / Retirer : lumiere et emetteur, une fois chacun", 0.f);

			// (x3)
			const ecs::NkEntityId l = NkEditeurPoserLumiere(m, NkVec2f(1.f, 1.f), NkTypeLumiere2D::NK_SPOT);
			const NkLumiere2D *ll = m.scene.Monde().Get<NkLumiere2D>(l);
			Temoin(ll != nullptr && ll->type == NkTypeLumiere2D::NK_SPOT && m.aSelection && m.selection == l &&
					   !m.scene.Eclairage().actif && std::strstr(m.message.CStr(), "eteint") != nullptr,
				   "(x3) poser une lumiere n'allume PAS la scene, et le dit", 0.f);

			// (x4)
			const ecs::NkEntityId f = NkEditeurPoserEffet(m, NkVec2f(-1.f, 1.f), NkPresetEffet2D::NK_FEU);
			const uint32 graine = m.scene.Monde().Get<NkEmetteur2D>(f)->graine;
			const bool applique = NkEditeurAppliquerPreset(m, f, NkPresetEffet2D::NK_NEIGE);
			const NkEmetteur2D *neige = m.scene.Monde().Get<NkEmetteur2D>(f);
			Temoin(applique && neige->preset == NkPresetEffet2D::NK_NEIGE && neige->graine == graine &&
					   neige->zone == NkZoneEmission2D::NK_LIGNE,
				   "(x4) preset applique (neige) : recette changee, graine gardee", static_cast<float32>(graine % 1000u));

			// (x5)
			const bool p1 = NkEditeurPoserPortee(m, l, 7.5f) && NkEditeurPortee(m, l) == 7.5f;
			const bool p2 = NkEditeurPoserPortee(m, l, 0.05f) && NkEditeurPortee(m, l) == 0.1f;
			NkEditeurAppliquerPreset(m, f, NkPresetEffet2D::NK_FEU);
			const bool p3 = NkEditeurPoserPortee(m, f, 3.f) && m.scene.Monde().Get<NkEmetteur2D>(f)->porteeLumiere == 3.f;
			Temoin(p1 && p2 && p3, "(x5) portee : 7,5 posee, 0,05 -> 0,1, emetteur -> sa lumiere liee", NkEditeurPortee(m, l));

			// (x6) (x7) sur la nuit d'exemple
			NkEditeurSceneNuit(m);
			const ecs::NkEntityId lanterne = Par(m.scene, "Lumiere");
			const NkTransform2D *tl = lanterne.IsValid() ? m.scene.Monde().Get<NkTransform2D>(lanterne) : nullptr;
			m.scene.Camera().PoserViseur(NkRect{0.f, 0.f, 1000.f, 500.f});
			m.scene.Camera().PoserZoom(40.f);
			bool choisie = false;
			if (tl != nullptr) {
				choisie = NkEditeurChoisirSous(m, NkVec2f(tl->position.x + 0.1f, tl->position.y), nullptr) &&
						  m.selection == lanterne;
			}
			Temoin(choisie && m.scene.Eclairage().actif && Compter(m.scene, 0) == 3 && Compter(m.scene, 1) == 3,
				   "(x6) nuit : 3 lumieres, 3 emetteurs ; l'icone de la lanterne se clique", 0.f);

			const ecs::NkEntityId caisse = Par(m.scene, "Caisse_1");
			const NkVec2f avant = caisse.IsValid() ? m.scene.Monde().Get<NkTransform2D>(caisse)->position : NkVec2f(0.f, 0.f);
			for (int32 k = 0; k < 30; ++k) {
				NkEditeurAvancer(m, 1.f / 60.f);
			}
			const NkVec2f apres = caisse.IsValid() ? m.scene.Monde().Get<NkTransform2D>(caisse)->position : NkVec2f(1.f, 1.f);
			Temoin(caisse.IsValid() && m.scene.Effets().NbParticules() > 0u && avant.x == apres.x && avant.y == apres.y,
				   "(x7) apercu en edition : le feu brule, aucune caisse ne bouge",
				   static_cast<float32>(m.scene.Effets().NbParticules()));

			// (x8)
			const uint32 nAvant = NbEntites(m.scene);
			const uint32 ambiante = m.scene.Eclairage().ambiante;
			NkEditeurJouer(m);
			const bool videAuDepart = m.scene.Effets().NbParticules() == 0u;
			for (int32 k = 0; k < 40; ++k) {
				NkEditeurAvancer(m, 1.f / 60.f);
			}
			const uint32 enJeu = m.scene.Effets().NbParticules();
			m.scene.Eclairage().ambiante = 0x000000FFu; // un jeu fait tomber la nuit noire
			m.scene.Monde().Query<NkLumiere2D>().ForEach([](ecs::NkEntityId, NkLumiere2D &x) { x.actif = false; });
			NkEditeurArreter(m);
			int32 allumees = 0;
			m.scene.Monde().Query<NkLumiere2D>().ForEach([&allumees](ecs::NkEntityId, NkLumiere2D &x) { allumees += x.actif ? 1 : 0; });
			Temoin(videAuDepart && enJeu > 0u && m.scene.Eclairage().ambiante == ambiante && allumees == 3 &&
					   m.scene.Effets().NbParticules() == 0u && NbEntites(m.scene) == nAvant,
				   "(x8) Jouer puis Arreter : ambiante et lumieres rendues, effets a zero", static_cast<float32>(enJeu));

			// (x9)
			m.chemin = "unkeny_editeur_banc_nuit.nkscene";
			const NkEclairage2D ec = m.scene.Eclairage();
			const bool sauve = NkEditeurSauver(m);
			NkEditeurNouvelleScene(m);
			const bool ouvert = NkEditeurOuvrir(m);
			std::remove("unkeny_editeur_banc_nuit.nkscene");
			const NkEclairage2D &eo = m.scene.Eclairage();
			Temoin(sauve && ouvert && Compter(m.scene, 0) == 3 && Compter(m.scene, 1) == 3 && eo.actif == ec.actif &&
					   eo.ambiante == ec.ambiante && eo.ombres == ec.ombres,
				   "(x9) Enregistrer puis Ouvrir la nuit : lumieres, emetteurs, eclairage", static_cast<float32>(NbEntites(m.scene)));

			// (x10)
			const ecs::NkEntityId leFeu = Par(m.scene, "Feu");
			bool autreGraine = false;
			if (leFeu.IsValid()) {
				m.selection = leFeu;
				m.aSelection = true;
				const ecs::NkEntityId copie = NkEditeurDupliquer(m);
				const NkEmetteur2D *a = m.scene.Monde().Get<NkEmetteur2D>(leFeu);
				const NkEmetteur2D *b = copie.IsValid() ? m.scene.Monde().Get<NkEmetteur2D>(copie) : nullptr;
				autreGraine = a != nullptr && b != nullptr && a->graine != b->graine && a->preset == b->preset;
			}
			Temoin(autreGraine, "(x10) dupliquer un feu : la copie a une autre graine", 0.f);

			memory::NkGetDefaultAllocator().Delete(pm);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC LUMIERE EDITEUR REUSSI" : "BANC LUMIERE EDITEUR EN ECHEC",
						gR, gE, gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
