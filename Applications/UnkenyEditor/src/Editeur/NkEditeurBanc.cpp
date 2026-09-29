// =============================================================================
// NkEditeurBanc.cpp — `UnkenyEditor --selftest` : les ACTIONS de l'editeur
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (e1)  la scene neuve porte sol, murs, caisses TEXTUREES, et de la matiere
//   (e2)  « Poser » pose un acteur de chaque rubrique et le selectionne
//   (e3)  « Choisir sous » : la matiere d'abord (le blob), puis les sprites (la
//         caisse), puis les formes sans sprite (le sol) ; (e3n) le vide : rien
//   (e4)  « Deplacer » : le blob arrive a sa cible (son centre de matiere), la
//         caisse aussi, et son CORPS RIGIDE avec elle (pas seulement le dessin)
//   (e5)  Jouer 1 s fait bouger la scene ; Arreter rend la photo, repasse en
//         EDITION et oublie la selection (ses identifiants sont morts)
//   (e6)  ActualiserCorps : une masse changee dans l'inspecteur arrive au
//         solveur (20 -> 5 kg), la vitesse en cours est gardee
//   (e7)  Enregistrer puis Ouvrir rend autant d'entites et de particules
//   (e8)  Supprimer la selection retire l'entite ET sa matiere
//   (e9)  « + Entite » : une entite VIDE (transform et nom seulement) ; on lui
//         ajoute un Corps rigide : le collisionneur vient avec, le corps tombe
//   (e10) sur une autre entite vide, « Corps mou > Eau » : la matiere nait A SA
//         position ; (e10n) un corps rigide y est alors refuse
//   (e11) retirer le Corps mou : la matiere part, l'ENTITE RESTE ; retirer le
//         Collisionneur d'un rigide retire le corps avec lui
//   (e12) dupliquer une caisse : meme composants, corps propre, decalee
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"

#include <cstdio>

namespace nkentseu {
	namespace editeur {

		namespace {
			int32 gE = 0, gR = 0;
			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}
			uint32 NbEntites(NkScene &s) {
				NkVector<ecs::NkEntityId> ids;
				s.Entites(ids);
				return static_cast<uint32>(ids.Size());
			}
			ecs::NkEntityId Par(NkScene &s, const char *prefixe) {
				ecs::NkEntityId t = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					usize k = 0;
					while (prefixe[k] != '\0' && e.nom[k] == prefixe[k]) {
						++k;
					}
					if (prefixe[k] == '\0' && !t.IsValid()) {
						t = id;
					}
				});
				return t;
			}
		} // namespace

		int32 NkEditeurLancerBanc() {
			gE = gR = 0;
			std::printf("\nUnkenyEditor — banc des actions de l'editeur\n\n");
			NkEditeurModele *pm = memory::NkGetDefaultAllocator().New<NkEditeurModele>(); // gros : sur le tas
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);

			// (e1)
			const ecs::NkEntityId caisse = Par(m.scene, "Caisse");
			const NkSprite2D *sc = caisse.IsValid() ? m.scene.Monde().Get<NkSprite2D>(caisse) : nullptr;
			Temoin(Par(m.scene, "Sol").IsValid() && sc != nullptr && sc->texId == m.ressources.texCaisse && sc->texId != 0u &&
					   m.scene.Particules()->particules.Size() > 100u,
				   "(e1) scene neuve : sol, caisses texturees, matiere (particules)", static_cast<float32>(m.scene.Particules()->particules.Size()));

			// (e2)
			const NkActeurSim chaque[5] = {NkActeurSim::NK_SLIME, NkActeurSim::NK_MIEL, NkActeurSim::NK_CRISTAL, NkActeurSim::NK_CORDE,
										   NkActeurSim::NK_BALLE};
			int32 poses = 0;
			for (int32 k = 0; k < 5; ++k) {
				m.acteur = chaque[k];
				const ecs::NkEntityId e = NkEditeurPoser(m, NkVec2f(-8.f + static_cast<float32>(k) * 3.5f, 3.f));
				poses += (e.IsValid() && m.aSelection && m.selection == e) ? 1 : 0;
			}
			Temoin(poses == 5, "(e2) un acteur de chaque rubrique pose et selectionne", static_cast<float32>(poses));

			// (e3)
			const ecs::NkEntityId blob = Par(m.scene, "Blob");
			NkVec2f cBlob(0.f, 0.f);
			{
				const physics::NkParticules2D *p = m.scene.Particules();
				const int32 ci = p->IndexCorps(m.scene.Monde().Get<NkCorpsMou2D>(blob)->corpsId);
				cBlob = p->CentreCorps(static_cast<uint32>(ci));
			}
			const bool surBlob = NkEditeurChoisirSous(m, cBlob) && m.selection == blob;
			const NkVec2f pc = m.scene.Monde().Get<NkTransform2D>(caisse)->position;
			const bool surCaisse = NkEditeurChoisirSous(m, pc) && m.selection == caisse;
			const bool surSol = NkEditeurChoisirSous(m, NkVec2f(0.f, -4.3f)) && m.selection == Par(m.scene, "Sol");
			Temoin(surBlob && surCaisse && surSol, "(e3) matiere, puis sprite, puis forme sans sprite", static_cast<float32>(surBlob + surCaisse + surSol));
			Temoin(!NkEditeurChoisirSous(m, NkVec2f(0.f, 40.f)) && !m.aSelection, "(e3n) dans le vide : rien de choisi", 0.f);

			// (e4)
			NkEditeurChoisirSous(m, cBlob);
			NkEditeurDeplacer(m, cBlob + NkVec2f(2.f, 1.f));
			NkVec2f apres(0.f, 0.f);
			NkEditeurCentreSelection(m, apres);
			const float32 ecartBlob = math::NkAbs(apres.x - cBlob.x - 2.f) + math::NkAbs(apres.y - cBlob.y - 1.f);
			NkEditeurChoisirSous(m, pc);
			NkEditeurDeplacer(m, NkVec2f(-3.f, 2.f));
			const physics::NkRigidBody *rb = m.scene.MondePhysique()->GetBody(m.scene.Monde().Get<NkCorps2D>(caisse)->corpsId);
			Temoin(ecartBlob < 1.0e-3f && math::NkAbs(rb->position.x + 3.f) < 1.0e-4f && math::NkAbs(rb->position.y - 2.f) < 1.0e-4f,
				   "(e4) blob et caisse a leur cible, le corps rigide aussi (ecart m)", ecartBlob);

			// (e5)
			char nomCaisse[32];
			std::snprintf(nomCaisse, sizeof(nomCaisse), "%s", m.scene.Monde().Get<NkEtiquette>(caisse)->nom);
			const uint32 nAvant = NbEntites(m.scene);
			NkEditeurJouer(m);
			for (int32 k = 0; k < 60; ++k) {
				NkEditeurAvancer(m, 1.f / 60.f);
			}
			const float32 yJeu = m.scene.Monde().Get<NkTransform2D>(caisse)->position.y;
			const bool bouge = yJeu < 1.9f;
			NkEditeurChoisirSous(m, NkVec2f(0.f, -4.3f));
			NkEditeurArreter(m);
			const ecs::NkEntityId caisse2 = Par(m.scene, nomCaisse);
			const float32 yRendu = caisse2.IsValid() ? m.scene.Monde().Get<NkTransform2D>(caisse2)->position.y : -99.f;
			Temoin(bouge && math::NkAbs(yRendu - 2.f) < 1.0e-4f && m.etat == NkEtatJeu::NK_EDITION && !m.aSelection &&
					   NbEntites(m.scene) == nAvant,
				   "(e5) Jouer fait tomber la caisse ; Arreter la rend (y m)", yRendu);

			// (e6)
			NkEditeurChoisirSous(m, NkVec2f(-3.f, 2.f));
			const ecs::NkEntityId sel = m.selection;
			m.scene.PoserVitesse(sel, NkVec2f(2.f, 0.f));
			NkCorps2D *k = m.scene.Monde().Get<NkCorps2D>(sel);
			const bool estCaisse = k != nullptr && k->masse == 20.f;
			if (k != nullptr) {
				k->masse = 5.f;
			}
			const bool maj = m.scene.ActualiserCorps(sel);
			const physics::NkRigidBody *r2 = m.scene.MondePhysique()->GetBody(m.scene.Monde().Get<NkCorps2D>(sel)->corpsId);
			Temoin(estCaisse && maj && math::NkAbs(r2->invMass - 0.2f) < 1.0e-5f && math::NkAbs(r2->linearVelocity.x - 2.f) < 1.0e-4f,
				   "(e6) masse changee : 5 kg au solveur, vitesse gardee (1/m)", r2->invMass);

			// (e7)
			m.chemin = "unkeny_editeur_banc.nkscene";
			const uint32 nE = NbEntites(m.scene);
			const uint32 nP = static_cast<uint32>(m.scene.Particules()->particules.Size());
			const bool sauve = NkEditeurSauver(m);
			NkEditeurNouvelleScene(m); // on la vide, pour que l'ouverture ait quelque chose a prouver
			const bool ouvert = NkEditeurOuvrir(m);
			std::remove("unkeny_editeur_banc.nkscene");
			Temoin(sauve && ouvert && NbEntites(m.scene) == nE && m.scene.Particules()->particules.Size() == nP,
				   "(e7) Enregistrer puis Ouvrir : memes entites et particules", static_cast<float32>(nE));

			// (e8)
			const ecs::NkEntityId b2 = Par(m.scene, "Blob");
			m.selection = b2;
			m.aSelection = b2.IsValid();
			const uint32 pAvant = static_cast<uint32>(m.scene.Particules()->particules.Size());
			const uint32 corpsId = b2.IsValid() ? m.scene.Monde().Get<NkCorpsMou2D>(b2)->corpsId : 0u;
			NkEditeurSupprimerSelection(m);
			Temoin(b2.IsValid() && !m.scene.Monde().IsAlive(b2) && m.scene.Particules()->IndexCorps(corpsId) < 0 &&
					   m.scene.Particules()->particules.Size() < pAvant,
				   "(e8) supprimer : l'entite ET sa matiere", static_cast<float32>(pAvant - m.scene.Particules()->particules.Size()));

			// (e9)
			const ecs::NkEntityId vide = NkEditeurCreerEntite(m, "Vide", NkVec2f(0.f, 9.f)); // au-dessus de tout : rien ne la pousse
			ecs::NkWorld &w = m.scene.Monde();
			const bool nue = w.Has<NkTransform2D>(vide) && w.Has<NkEtiquette>(vide) && !w.Has<NkSprite2D>(vide) && !w.Has<NkCollisionneur2D>(vide) &&
							 !w.Has<NkCorps2D>(vide) && !w.Has<NkCorpsMou2D>(vide) && m.selection == vide;
			const bool ajoute = NkEditeurAjouterComposant(m, vide, NkComposantEditeur::NK_CORPS);
			m.etat = NkEtatJeu::NK_JEU;
			for (int32 i = 0; i < 30; ++i) {
				NkEditeurAvancer(m, 1.f / 60.f);
			}
			m.etat = NkEtatJeu::NK_EDITION;
			const float32 yVide = w.Get<NkTransform2D>(vide)->position.y;
			Temoin(nue && ajoute && w.Has<NkCollisionneur2D>(vide) && yVide < 8.8f && yVide > 7.f, "(e9) entite vide, + Corps rigide (et son collisionneur) : elle tombe",
				   yVide);

			// (e10)
			const ecs::NkEntityId flaque = NkEditeurCreerEntite(m, "Flaque", NkVec2f(6.f, 2.f));
			const uint32 p0 = static_cast<uint32>(m.scene.Particules()->particules.Size());
			const bool mouOk = NkEditeurAjouterComposant(m, flaque, NkComposantEditeur::NK_CORPS_MOU, NkActeurSim::NK_EAU);
			NkVec2f cf(0.f, 0.f);
			NkEditeurCentreSelection(m, cf);
			Temoin(mouOk && m.scene.Particules()->particules.Size() > p0 && math::NkAbs(cf.x - 6.f) < 0.1f && math::NkAbs(cf.y - 2.f) < 0.1f,
				   "(e10) + Corps mou > Eau : la matiere nait a la position de l'entite", static_cast<float32>(m.scene.Particules()->particules.Size() - p0));
			Temoin(!NkEditeurPeutAjouter(m, flaque, NkComposantEditeur::NK_CORPS) && !NkEditeurAjouterComposant(m, flaque, NkComposantEditeur::NK_CORPS),
				   "(e10n) corps rigide ET corps mou : refuse", 0.f);

			// (e11)
			const bool retire = NkEditeurRetirerComposant(m, flaque, NkComposantEditeur::NK_CORPS_MOU);
			NkEditeurAvancer(m, 1.f / 60.f);
			m.scene.Pas(1.f / 60.f); // la synchro des corps mous ne doit pas tuer l'entite
			const bool resteMou = w.IsAlive(flaque) && !w.Has<NkCorpsMou2D>(flaque) && m.scene.Particules()->particules.Size() == p0;
			const bool retireCol = NkEditeurRetirerComposant(m, vide, NkComposantEditeur::NK_COLLISIONNEUR);
			const bool resteRig = w.IsAlive(vide) && !w.Has<NkCorps2D>(vide) && !w.Has<NkCollisionneur2D>(vide);
			Temoin(retire && resteMou && retireCol && resteRig, "(e11) retirer matiere / collisionneur : l'entite reste, le corps part", 0.f);

			// (e12)
			const ecs::NkEntityId uneCaisse = Par(m.scene, "Caisse");
			m.selection = uneCaisse;
			m.aSelection = true;
			const ecs::NkEntityId copie = NkEditeurDupliquer(m);
			const NkCorps2D *ko = w.Get<NkCorps2D>(uneCaisse);
			const NkCorps2D *kc = copie.IsValid() ? w.Get<NkCorps2D>(copie) : nullptr;
			const NkTransform2D *to = w.Get<NkTransform2D>(uneCaisse);
			const NkTransform2D *tc = copie.IsValid() ? w.Get<NkTransform2D>(copie) : nullptr;
			Temoin(kc != nullptr && kc->corpsId != ko->corpsId && kc->masse == ko->masse && w.Has<NkSprite2D>(copie) &&
					   math::NkAbs(tc->position.x - to->position.x - 0.5f) < 1.0e-4f,
				   "(e12) dupliquer une caisse : ses composants, son propre corps", kc != nullptr ? kc->masse : -1.f);

			memory::NkGetDefaultAllocator().Delete(pm);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC EDITEUR REUSSI" : "BANC EDITEUR EN ECHEC", gR, gE, gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
