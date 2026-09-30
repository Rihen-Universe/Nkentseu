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
//   (e13) le clic de selection : sur une caisse, elle est choisie ; dans le
//         vide, la selection est VIDEE (ChoisirSous, lui, la laisse)
//   (e14) « Cadrer » : la vue se centre sur la selection ; sans selection,
//         rien ne bouge et l'action le dit (false)
//   (e15) le glisser deplace en EDITION, et ni en JEU ni en PAUSE
//   (e16) « Tourner » d'un quart de tour : la caisse (son transform, et son
//         corps est toujours la) ; le tissu (sa MATIERE : la boite echange
//         largeur et hauteur)
//   (e17) « Echelle » x2 : sprite ET collisionneur de la caisse ; la boite du
//         tissu double, et la longueur de REPOS de ses liens aussi
//   (e18) « Accrocher » : au pas le plus proche, des deux cotes de zero ; un
//         pas nul laisse la valeur intacte
//   (e19) « Zone a cadrer » : centree sur la selection ; sans selection, elle
//         couvre TOUTE la scene (le sol y est)
//   (eh1) (2026-09-29) « Rattacher » (le glisser de l'Outliner) : l'enfant ne
//         bouge pas, une boucle est refusee ; en EDITION, deplacer le parent
//         emporte l'enfant a la trame suivante
//   (ep1) « Creer un prefab » : le .nkprefab est ecrit a cote de la scene, une
//         instance nait avec son enfant ; apres Enregistrer / Ouvrir, les deux
//         instances (la source et la copie) sont reliees au prefab et la
//         hierarchie est la
//   (e40) la souris (NkEditeurSouris) contre un VRAI NkGuiInput : un appui et
//         son relachement entre deux trames font un clic ; un relachement puis
//         un appui entre deux trames font un relachement PUIS un clic.
//         Contre-epreuve : l'etat brut ecrit tel quel perd les deux.
//   (e41) la prise au clic (NkEditeurPrendreSous), chaque cas contre l'ANCIENNE
//         prise recopiee ici : petit sprite a 3 px de son bord (22 px/m) ;
//         planche tournee, prise sur elle et pas dans sa boite droite ; sprites
//         superposes (la couche du dessus, le petit sur le grand) ; caisse
//         collee a un blob a 122 px/m (la caisse, pas le blob) ; tissu pris
//         entre ses particules, pas dans une maille dechiree ; milieu d'un
//         ballon ; bord du sol a 3 px ; entite vide (son marqueur) ; le vide ;
//         cadenas et oeil (cache en EDITION seulement)
//   (e42) l'oeil et le cadenas survivent a Jouer / Arreter et a Enregistrer /
//         Ouvrir ; un fichier qui ne les porte pas (celui d'avant) se relit
//         tel quel, sans drapeau
//   (e43) l'Outliner, dessine dans un VRAI NkGuiContext, gestes rejoues : F2
//         ouvre la saisie en place, nom choisi, la frappe le REMPLACE, Entree
//         l'ecrit ; Echap l'abandonne ; le clic LENT sur le nom de la ligne
//         choisie l'ouvre, le double-clic non (il cadre) ; l'oeil et le cadenas
//         basculent au clic, et une entite verrouillee se choisit TOUJOURS dans
//         l'Outliner
//   (e44) la hierarchie : un parent CACHE cache son enfant dans la vue, un
//         parent VERROUILLE le fige -- sans toucher aux drapeaux de l'enfant ;
//         rouverts, l'enfant se reprend
//   (e45) les cartes de l'inspecteur : « Reinitialiser » le sprite d'une caisse
//         rend 1 x 1 m et GARDE sa texture ; « Copier / Coller » le corps d'une
//         caisse sur une autre donne la masse au SOLVEUR sans lui prendre son
//         corps ; coller sur une entite sans ce composant est refuse ; le Transform colle
//         teleporte le corps ; « Monter » saute les cartes que l'entite n'a pas
//   (e46) l'icone d'une LUMIERE posee sur une caisse l'emporte sur la caisse
//         (elle est peinte par-dessus) ; cachee, on prend la caisse dessous ;
//         une lumiere n'a pas de losange « entite vide » en plus de son icone
//   (e47) le Transform d'Unreal : l'echelle 2 x 2 double sprite ET collisionneur
//         et se relit 2 x 2 ; le gizmo x1,5 la porte a 3 ; remise a 1, les
//         dimensions reviennent et le composant d'echelle s'en va. Les cases :
//         collisionneur eteint = couche et masque a 0, rallume = rendus ; corps
//         eteint = cinematique sans gravite, rallume = dynamique ; et l'etat
//         eteint traverse Jouer / Arreter
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurLumiere.h"
#include "Editeur/NkEditeurSouris.h"

#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiFont.h"

#include <cstdio>
#include <cstring>

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

			/// L'ANCIENNE prise (NkEditeurChoisirSous et NkEntiteSous jusqu'au
			/// 29/09), recopiee telle quelle pour la CONTRE-EPREUVE de e41 : des
			/// marges fixes en metres, la matiere avant tout, les sprites dans leur
			/// boite droite.
			bool AnciennePrise(NkScene &s, const NkVec2f &monde, ecs::NkEntityId &sortie) {
				if (physics::NkParticules2D *p = s.Particules()) {
					const int32 i = p->ParticuleProche(monde, 0.25f);
					if (i >= 0) {
						const uint32 id = p->corps[p->particules[static_cast<uint32>(i)].corps].id;
						const ecs::NkEntityId e = s.EntiteDuCorpsMou(id);
						if (e.IsValid()) {
							sortie = e;
							return true;
						}
					}
				}
				bool trouve = false;
				int32 meilleureCouche = -1000000;
				s.Monde().Query<NkTransform2D, NkSprite2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t, NkSprite2D &sp) {
					if (!sp.visible) {
						return;
					}
					const float32 hw = sp.taille.x * math::NkAbs(t.echelle.x) * 0.5f;
					const float32 hh = sp.taille.y * math::NkAbs(t.echelle.y) * 0.5f;
					if (monde.x < t.position.x - hw || monde.x > t.position.x + hw || monde.y < t.position.y - hh ||
						monde.y > t.position.y + hh) {
						return;
					}
					if (sp.couche >= meilleureCouche) {
						meilleureCouche = sp.couche;
						sortie = id;
						trouve = true;
					}
				});
				if (trouve) {
					return true;
				}
				float32 meilleur = 0.1f;
				s.Monde().Query<NkTransform2D, NkCollisionneur2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t, NkCollisionneur2D &col) {
					const float32 d = NkDistanceForme2D(t, col, monde);
					if (d < meilleur) {
						meilleur = d;
						sortie = id;
						trouve = true;
					}
				});
				return trouve;
			}

			/// Le fichier contient-il ce texte ? (La cle d'un composant dans une
			/// scene enregistree.)
			bool FichierContient(const char *chemin, const char *texte) {
				std::FILE *f = std::fopen(chemin, "rb");
				if (f == nullptr) {
					return false;
				}
				NkVector<char> octets;
				char tampon[4096];
				usize lus = 0;
				while ((lus = std::fread(tampon, 1, sizeof(tampon), f)) > 0) {
					for (usize i = 0; i < lus; ++i) {
						octets.PushBack(tampon[i]);
					}
				}
				std::fclose(f);
				const usize n = std::strlen(texte);
				for (usize i = 0; n > 0 && i + n <= octets.Size(); ++i) {
					if (std::memcmp(octets.Data() + i, texte, n) == 0) {
						return true;
					}
				}
				return false;
			}

			uint32 NbDrapeaux(NkScene &s) {
				uint32 n = 0;
				s.Monde().Query<NkDrapeauxEditeur>().ForEach([&](ecs::NkEntityId, NkDrapeauxEditeur &) { ++n; });
				return n;
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

			// (e13)
			const NkVec2f surLaCaisse = to->position;
			const bool touchee = NkEditeurCliquerSelection(m, surLaCaisse);
			const bool cEstElle = m.aSelection && m.selection == uneCaisse;
			const bool vide13 = !NkEditeurCliquerSelection(m, NkVec2f(0.f, 40.f));
			Temoin(touchee && cEstElle && vide13 && !m.aSelection, "(e13) clic : sur une caisse elle est choisie ; dans le vide, plus rien", 0.f);

			// (e14)
			m.selection = uneCaisse;
			m.aSelection = true;
			m.scene.Camera().PoserCentre(NkVec2f(-30.f, -30.f));
			const bool cadree = NkEditeurCadrerSelection(m);
			const NkVec2f vue = m.scene.Camera().Centre();
			NkVec2f attendu(0.f, 0.f);
			NkEditeurCentreSelection(m, attendu);
			const float32 ecart = math::NkAbs(vue.x - attendu.x) + math::NkAbs(vue.y - attendu.y);
			m.aSelection = false;
			m.scene.Camera().PoserCentre(NkVec2f(-30.f, -30.f));
			const bool rienSansSel = !NkEditeurCadrerSelection(m) && m.scene.Camera().Centre().x == -30.f;
			Temoin(cadree && ecart < 1.0e-4f && rienSansSel, "(e14) cadrer : la vue se centre sur la selection ; sans elle, rien", ecart);

			// (e15)
			const bool enEdition = NkEditeurPeutDeplacer(m);
			NkEditeurJouer(m);
			const bool pasEnJeu = !NkEditeurPeutDeplacer(m);
			NkEditeurPause(m);
			const bool pasEnPause = !NkEditeurPeutDeplacer(m);
			NkEditeurArreter(m);
			Temoin(enEdition && pasEnJeu && pasEnPause && NkEditeurPeutDeplacer(m), "(e15) le glisser deplace en EDITION, ni en jeu ni en pause", 0.f);

			// (e16) tourner : une caisse (transform + corps refait), un tissu (matiere)
			const ecs::NkEntityId aTourner = Par(m.scene, "Caisse");
			m.selection = aTourner;
			m.aSelection = true;
			const float32 rot0 = w.Get<NkTransform2D>(aTourner)->rotation;
			const bool tourneRig = NkEditeurTourner(m, 1.5707963f);
			const NkTransform2D *tr16 = w.Get<NkTransform2D>(aTourner);
			const bool rigOk = tourneRig && math::NkAbs(tr16->rotation - rot0 - 1.5707963f) < 1.0e-4f && w.Has<NkCorps2D>(aTourner);
			m.acteur = NkActeurSim::NK_TISSU;
			m.acteurSimple = false;
			const ecs::NkEntityId tissu = NkEditeurPoser(m, NkVec2f(-6.f, 7.f));
			NkVec2f a0(0.f, 0.f);
			NkVec2f b0(0.f, 0.f);
			NkEditeurBoiteSelection(m, a0, b0);
			const bool tourneMou = NkEditeurTourner(m, 1.5707963f);
			NkVec2f a1(0.f, 0.f);
			NkVec2f b1(0.f, 0.f);
			NkEditeurBoiteSelection(m, a1, b1);
			const float32 l0 = b0.x - a0.x;
			const float32 h0 = b0.y - a0.y;
			const float32 l1 = b1.x - a1.x;
			const float32 h1 = b1.y - a1.y;
			// Un quart de tour ECHANGE largeur et hauteur de la boite du tissu.
			const bool mouOk16 = tissu.IsValid() && tourneMou && math::NkAbs(l1 - h0) < 0.05f && math::NkAbs(h1 - l0) < 0.05f && l0 != h0;
			Temoin(rigOk && mouOk16, "(e16) tourner d'un quart : caisse (et son corps), tissu (sa matiere)", l1 - h0);

			// (e17) mettre a l'echelle x2 : sprite + collisionneur ; matiere + ses liens
			m.selection = aTourner;
			m.aSelection = true;
			const NkVec2f taille0 = w.Get<NkSprite2D>(aTourner)->taille;
			const NkVec2f demi0 = w.Get<NkCollisionneur2D>(aTourner)->demiTaille;
			const bool echRig = NkEditeurMettreAEchelle(m, NkVec2f(2.f, 2.f));
			const NkSprite2D *sp17 = w.Get<NkSprite2D>(aTourner);
			const NkCollisionneur2D *co17 = w.Get<NkCollisionneur2D>(aTourner);
			const bool rig17 = echRig && math::NkAbs(sp17->taille.x - taille0.x * 2.f) < 1.0e-4f &&
							   math::NkAbs(co17->demiTaille.y - demi0.y * 2.f) < 1.0e-4f && w.Has<NkCorps2D>(aTourner);
			m.selection = tissu;
			m.aSelection = true;
			const NkCorpsMou2D *mou17 = w.Get<NkCorpsMou2D>(tissu);
			physics::NkParticules2D *p17 = m.scene.Particules();
			const int32 ci17 = (mou17 != nullptr && p17 != nullptr) ? p17->IndexCorps(mou17->corpsId) : -1;
			float32 repos0 = -1.f;
			uint32 lienTemoin = 0u;
			for (uint32 k = 0; ci17 >= 0 && k < p17->liens.Size() && repos0 < 0.f; ++k) {
				if (p17->liens[k].corps == static_cast<uint32>(ci17)) {
					repos0 = p17->liens[k].repos;
					lienTemoin = k;
				}
			}
			NkEditeurBoiteSelection(m, a0, b0);
			const bool echMou = NkEditeurMettreAEchelle(m, NkVec2f(2.f, 2.f));
			NkEditeurBoiteSelection(m, a1, b1);
			const float32 rapport = (b1.x - a1.x) / ((b0.x - a0.x) > 1.0e-4f ? (b0.x - a0.x) : 1.f);
			const bool mou17ok = echMou && repos0 > 0.f && math::NkAbs(p17->liens[lienTemoin].repos - repos0 * 2.f) < 1.0e-4f &&
								 rapport > 1.8f && rapport < 2.2f;
			Temoin(rig17 && mou17ok, "(e17) echelle x2 : sprite ET collisionneur ; matiere ET repos des liens", rapport);

			// (e18) l'accrochage
			const bool acc = NkEditeurAccrocher(0.74f, 0.5f) == 0.5f && NkEditeurAccrocher(-0.26f, 0.5f) == -0.5f &&
							 math::NkAbs(NkEditeurAccrocher(17.f, 15.f) - 15.f) < 1.0e-5f && NkEditeurAccrocher(0.33f, 0.f) == 0.33f;
			Temoin(acc, "(e18) accrocher : au pas le plus proche ; pas nul, valeur intacte", NkEditeurAccrocher(0.74f, 0.5f));

			// (e19) la zone a cadrer : la selection, sinon TOUTE la scene
			NkVec2f zc(0.f, 0.f);
			NkVec2f zt(0.f, 0.f);
			const bool zSel = NkEditeurZoneACadrer(m, zc, zt);
			NkVec2f ct(0.f, 0.f);
			NkEditeurCentreSelection(m, ct);
			const bool surSel = zSel && math::NkAbs(zc.x - ct.x) < 0.5f && math::NkAbs(zc.y - ct.y) < 0.5f;
			m.aSelection = false;
			const bool zTout = NkEditeurZoneACadrer(m, zc, zt);
			const NkTransform2D *tSol = w.Get<NkTransform2D>(Par(m.scene, "Sol"));
			const bool couvreSol = zTout && tSol != nullptr && math::NkAbs(tSol->position.x - zc.x) < zt.x * 0.5f &&
								   math::NkAbs(tSol->position.y - zc.y) < zt.y * 0.5f && !m.aSelection;
			Temoin(surSel && couvreSol, "(e19) zone a cadrer : la selection ; sans elle, toute la scene", zt.x);

			// (eh1) la hierarchie dans l'editeur (2026-09-29)
			const ecs::NkEntityId pere = NkEditeurCreerEntite(m, "Pere", NkVec2f(20.f, 20.f));
			const ecs::NkEntityId fils = NkEditeurCreerEntite(m, "Fils", NkVec2f(21.f, 20.f));
			const bool rattache = NkEditeurRattacher(m, fils, pere);
			const bool boucleRefusee = !NkEditeurRattacher(m, pere, fils);
			w.Get<NkTransform2D>(pere)->position = NkVec2f(25.f, 20.f); // le geste d'un gizmo
			NkEditeurAvancer(m, 1.f / 60.f);							 // en EDITION : rien ne fait Pas
			const NkVec2f pFils = w.Get<NkTransform2D>(fils)->position;
			Temoin(m.etat == NkEtatJeu::NK_EDITION && rattache && boucleRefusee && math::NkAbs(pFils.x - 26.f) < 1.0e-4f &&
					   math::NkAbs(pFils.y - 20.f) < 1.0e-4f,
				   "(eh1) rattacher (boucle refusee) ; en EDITION le parent emporte l'enfant", pFils.x);

			// (ep1) « Creer un prefab » depuis la selection, instancier, enregistrer, rouvrir
			m.chemin = "unkeny_editeur_banc_h.nkscene";
			m.selection = pere;
			m.aSelection = true;
			const uint32 idPrefab = NkEditeurCreerPrefab(m);
			const NkString cheminPrefab = NkEditeurCheminPrefab(m, "Pere");
			const bool fichierPrefab = !NkFile::ReadAllText(cheminPrefab.CStr()).Empty();
			const ecs::NkEntityId inst = m.prefabs.Instancier(m.scene, idPrefab, NkVec2f(30.f, 20.f));
			NkVector<ecs::NkEntityId> enfantsInst;
			m.scene.Enfants(inst, enfantsInst);
			const bool sauveH = NkEditeurSauver(m);
			const bool ouvertH = NkEditeurOuvrir(m);
			NkVector<ecs::NkEntityId> instances;
			m.prefabs.Instances(m.scene, idPrefab, instances);
			const ecs::NkEntityId fils2 = Par(m.scene, "Fils");
			std::remove("unkeny_editeur_banc_h.nkscene");
			std::remove(cheminPrefab.CStr());
			Temoin(idPrefab != 0u && fichierPrefab && enfantsInst.Size() == 1u && sauveH && ouvertH && instances.Size() == 2u &&
					   fils2.IsValid() && m.scene.Parent(fils2).IsValid(),
				   "(ep1) prefab de la selection : fichier, instance, et lien rouvert", static_cast<float32>(instances.Size()));

			// (e40) la souris, sans clic perdu. Une trame = NewFrame (ce que NKGui
			// lit), puis FinDeTrame. La contre-epreuve ecrit l'etat brut, comme
			// l'editeur le faisait avant.
			{
				auto clicSec = [](bool retenue) {
					nkgui::NkGuiInput in;
					NkEditeurSouris s;
					in.NewFrame();
					// Appui ET relachement entre deux trames (clic sec, pave tactile).
					if (retenue) {
						s.Appui(in, 0);
						s.Relache(in, 0);
					} else {
						in.mouseDown[0] = true;
						in.mouseDown[0] = false;
					}
					in.NewFrame();
					const bool clic = in.mouseClicked[0];
					s.FinDeTrame(in);
					in.NewFrame();
					return clic && in.mouseReleased[0];
				};
				auto reappui = [](bool retenue) {
					nkgui::NkGuiInput in;
					NkEditeurSouris s;
					// Le bouton tenu, vu par une trame.
					if (retenue) {
						s.Appui(in, 0);
					} else {
						in.mouseDown[0] = true;
					}
					in.NewFrame();
					s.FinDeTrame(in);
					// Relachement PUIS appui entre deux trames (le 29/09, apres une
					// restauration de la fenetre).
					if (retenue) {
						s.Relache(in, 0);
						s.Appui(in, 0);
					} else {
						in.mouseDown[0] = false;
						in.mouseDown[0] = true;
					}
					in.NewFrame();
					const bool relache = in.mouseReleased[0];
					s.FinDeTrame(in);
					in.NewFrame();
					return relache && in.mouseClicked[0];
				};
				const bool avec = clicSec(true) && reappui(true);
				const bool sans = !clicSec(false) && !reappui(false);
				Temoin(avec && sans, "(e40) souris : clic sec et re-appui vus (brut : perdus)", static_cast<float32>(avec + sans));
			}

			// (e41) la prise au clic, sur une scene NEUVE : chaque cas mesure la
			// nouvelle prise ET l'ancienne (la contre-epreuve).
			NkEditeurNouvelleScene(m);
			{
				ecs::NkWorld &w21 = m.scene.Monde();
				NkVue2D &cam = m.scene.Camera();
				physics::NkParticules2D *p21 = m.scene.Particules();
				auto prend = [&](const NkVec2f &q, ecs::NkEntityId attendu) {
					ecs::NkEntityId e;
					return NkEditeurPrendreSous(m, q, e) && e == attendu;
				};
				auto rien = [&](const NkVec2f &q) {
					ecs::NkEntityId e;
					return !NkEditeurPrendreSous(m, q, e);
				};
				auto ancien = [&](const NkVec2f &q, ecs::NkEntityId attendu) {
					ecs::NkEntityId e;
					return AnciennePrise(m.scene, q, e) && e == attendu;
				};

				// (a) petit sprite, vue eloignee : un clic a 3 px de son bord.
				cam.PoserZoom(22.f);
				const ecs::NkEntityId petit = m.scene.Creer("Petit", NkVec2f(0.f, 20.f));
				NkSprite2D sp;
				sp.taille = NkVec2f(0.2f, 0.2f);
				w21.Add<NkSprite2D>(petit, sp);
				const NkVec2f a3(0.1f + 3.f / 22.f, 20.f);
				Temoin(prend(a3, petit) && !ancien(a3, petit), "(e41a) petit sprite, 22 px/m : pris a 3 px du bord (ancienne : non)", 3.f);

				// (b) une planche tournee de 45 degres : sur elle, et pas dans sa boite droite.
				const ecs::NkEntityId planche = m.scene.Creer("Planche", NkVec2f(6.f, 20.f));
				sp.taille = NkVec2f(2.f, 0.2f);
				w21.Add<NkSprite2D>(planche, sp);
				w21.Get<NkTransform2D>(planche)->rotation = 0.78539816f;
				const NkVec2f bout(6.f + 0.9f * 0.70710678f, 20.f + 0.9f * 0.70710678f);
				const NkVec2f fantome(6.9f, 20.05f);
				Temoin(prend(bout, planche) && rien(fantome) && !ancien(bout, planche) && ancien(fantome, planche),
					   "(e41b) planche tournee : prise sur elle, pas a cote (ancienne : l'inverse)", 45.f);

				// (c) superposes : la couche du dessus ; a couche egale, le petit sur le grand.
				const ecs::NkEntityId fond = m.scene.Creer("Fond", NkVec2f(12.f, 20.f));
				sp.taille = NkVec2f(2.f, 2.f);
				sp.couche = -5;
				w21.Add<NkSprite2D>(fond, sp);
				const ecs::NkEntityId dessus = m.scene.Creer("Dessus", NkVec2f(12.3f, 20.f));
				sp.taille = NkVec2f(0.4f, 0.4f);
				sp.couche = 3;
				w21.Add<NkSprite2D>(dessus, sp);
				const ecs::NkEntityId grand = m.scene.Creer("Grand", NkVec2f(16.f, 20.f));
				sp.taille = NkVec2f(2.f, 2.f);
				sp.couche = 0;
				w21.Add<NkSprite2D>(grand, sp);
				const ecs::NkEntityId pose = m.scene.Creer("Pose", NkVec2f(16.2f, 20.f));
				sp.taille = NkVec2f(0.3f, 0.3f);
				w21.Add<NkSprite2D>(pose, sp);
				Temoin(prend(NkVec2f(12.3f, 20.f), dessus) && prend(NkVec2f(11.5f, 20.f), fond) && prend(NkVec2f(16.2f, 20.f), pose) &&
						   prend(NkVec2f(15.5f, 20.f), grand),
					   "(e41c) superposes : couche du dessus ; a egalite, le petit", 3.f);

				// (d) une caisse collee a un blob, vue rapprochee : le clic DANS la
				// caisse la prend. L'ancienne prenait le blob (0,25 m = 30 px ici).
				cam.PoserZoom(122.f);
				const ecs::NkEntityId blob = NkPoserActeurSim(m.scene, NkActeurSim::NK_BLOB, NkVec2f(-6.f, 20.f), &m.ressources);
				const physics::NkCorpsP2D *cb = &p21->corps[static_cast<uint32>(p21->IndexCorps(w21.Get<NkCorpsMou2D>(blob)->corpsId))];
				float32 droite = -1e9f;
				float32 yDroite = 20.f;
				for (uint32 i = cb->debut; i < cb->debut + cb->nombre; ++i) {
					const float32 x = p21->particules[i].pos.x + p21->particules[i].rayon * 1.45f;
					if (x > droite) {
						droite = x;
						yDroite = p21->particules[i].pos.y;
					}
				}
				const ecs::NkEntityId caisse41 = NkPoserActeurSim(m.scene, NkActeurSim::NK_CAISSE, NkVec2f(droite + 0.1f + 0.33f, yDroite), &m.ressources);
				const NkVec2f dansCaisse(droite + 0.1f + 0.05f, yDroite);
				Temoin(prend(dansCaisse, caisse41) && ancien(dansCaisse, blob), "(e41d) caisse contre un blob, 122 px/m : la caisse (ancienne : le blob)",
					   (dansCaisse.x - droite) * 122.f);

				// (e) le tissu : entre ses particules, oui ; dans une maille DECHIREE, non.
				const ecs::NkEntityId tissu = Par(m.scene, "Tissu");
				const physics::NkCorpsP2D &ct = p21->corps[static_cast<uint32>(p21->IndexCorps(w21.Get<NkCorpsMou2D>(tissu)->corpsId))];
				const int32 nx = ct.nx;
				const int32 ny = ct.ny;
				const uint32 attendus = static_cast<uint32>(ny * (nx - 1) + (ny - 1) * nx);
				bool tissuOk = nx >= 3 && ny >= 3 && ct.lienNombre >= attendus;
				NkVec2f maille(0.f, 0.f);
				if (tissuOk) {
					const uint32 i = 1u;
					const uint32 j = 1u;
					const uint32 n = static_cast<uint32>(nx);
					maille = p21->particules[ct.debut + j * n + i].pos + p21->particules[ct.debut + j * n + i + 1u].pos +
							 p21->particules[ct.debut + (j + 1u) * n + i].pos + p21->particules[ct.debut + (j + 1u) * n + i + 1u].pos;
					maille = NkVec2f(maille.x * 0.25f, maille.y * 0.25f);
					const bool entier = prend(maille, tissu);
					// On dechire la maille (1, 1) : son lien horizontal du haut.
					p21->liens[ct.lienDebut + j * (n - 1u) + i].casse = true;
					tissuOk = entier && rien(maille) && ancien(maille, tissu);
				}
				Temoin(tissuOk, "(e41e) tissu : entre ses points oui, maille dechiree non (ancienne : oui)", maille.y);

				// (f) le MILIEU d'un ballon : plein a l'ecran, a plus de 0,25 m de son anneau.
				const ecs::NkEntityId ballon = Par(m.scene, "Ballon");
				const int32 ciBallon = p21->IndexCorps(w21.Get<NkCorpsMou2D>(ballon)->corpsId);
				const NkVec2f milieu = p21->CentreCorps(static_cast<uint32>(ciBallon));
				Temoin(prend(milieu, ballon) && !ancien(milieu, ballon), "(e41f) milieu d'un ballon : pris (ancienne : non)", milieu.x);

				// (g) le bord du sol, 3 px au-dessus, vue eloignee.
				cam.PoserZoom(22.f);
				const ecs::NkEntityId sol = Par(m.scene, "Sol");
				const NkVec2f auBord(-3.f, -4.f + 3.f / 22.f);
				Temoin(prend(auBord, sol) && !ancien(auBord, sol), "(e41g) bord du sol a 3 px, 22 px/m : pris (ancienne : non)", 3.f);

				// (h) une entite VIDE : son marqueur se prend ; le vide, rien.
				const ecs::NkEntityId vide = NkEditeurCreerEntite(m, "Vide", NkVec2f(20.f, 20.f));
				const NkVec2f surMarqueur(20.f + 4.f / 22.f, 20.f);
				ecs::NkEntityId aucun;
				Temoin(prend(surMarqueur, vide) && !ancien(surMarqueur, vide) && rien(NkVec2f(0.f, 40.f)) &&
						   !AnciennePrise(m.scene, NkVec2f(0.f, 40.f), aucun),
					   "(e41h) entite vide : son marqueur (ancienne : rien) ; le vide : rien", 4.f);

				// (i) le cadenas : la caisse ne se prend plus ; l'oeil ferme : le
				// blob non plus en EDITION, mais en jeu si (le jeu montre tout).
				cam.PoserZoom(122.f);
				NkEditeurVerrouiller(m, caisse41, true);
				const bool verrou = rien(dansCaisse);
				NkEditeurVerrouiller(m, caisse41, false);
				const bool rendu = prend(dansCaisse, caisse41);
				const NkVec2f cBlob21 = p21->CentreCorps(static_cast<uint32>(p21->IndexCorps(w21.Get<NkCorpsMou2D>(blob)->corpsId)));
				NkEditeurCacher(m, blob, true);
				const bool cacheEdition = rien(cBlob21);
				m.etat = NkEtatJeu::NK_JEU;
				const bool vuEnJeu = prend(cBlob21, blob);
				m.etat = NkEtatJeu::NK_EDITION;
				NkEditeurCacher(m, blob, false);
				Temoin(verrou && rendu && cacheEdition && vuEnJeu && !w21.Has<NkDrapeauxEditeur>(blob),
					   "(e41i) cadenas : pas pris ; oeil ferme : pas en EDITION, si en jeu", static_cast<float32>(verrou + rendu + cacheEdition + vuEnJeu));
			}

			// (e42) l'oeil et le cadenas voyagent avec la scene.
			{
				NkEditeurVerrouiller(m, Par(m.scene, "Sol"), true);
				NkEditeurCacher(m, Par(m.scene, "Ballon"), true);
				NkEditeurJouer(m);
				NkEditeurAvancer(m, 1.f / 60.f);
				NkEditeurArreter(m); // les identifiants changent : on retrouve par le nom
				const bool photo = NkEditeurEstVerrouille(m, Par(m.scene, "Sol")) && NkEditeurEstCache(m, Par(m.scene, "Ballon")) &&
								   NbDrapeaux(m.scene) == 2u;
				const char *chemin = "unkeny_editeur_banc_drapeaux.nkscene";
				m.chemin = chemin;
				const bool sauve = NkEditeurSauver(m);
				const bool avecCle = FichierContient(chemin, "UnkenyEditor.Drapeaux");
				NkEditeurNouvelleScene(m);
				const bool neuve = NbDrapeaux(m.scene) == 0u;
				const bool ouvert = NkEditeurOuvrir(m);
				const bool relu = ouvert && NkEditeurEstVerrouille(m, Par(m.scene, "Sol")) && !NkEditeurEstCache(m, Par(m.scene, "Sol")) &&
								  NkEditeurEstCache(m, Par(m.scene, "Ballon")) && !NkEditeurEstVerrouille(m, Par(m.scene, "Ballon")) &&
								  NbDrapeaux(m.scene) == 2u;
				// Un fichier SANS drapeaux -- ce qu'ecrivait l'editeur d'avant : relu tel quel.
				NkEditeurVerrouiller(m, Par(m.scene, "Sol"), false);
				NkEditeurCacher(m, Par(m.scene, "Ballon"), false);
				const uint32 nE = NbEntites(m.scene);
				NkEditeurSauver(m);
				const bool sansCle = !FichierContient(chemin, "UnkenyEditor.Drapeaux");
				NkEditeurNouvelleScene(m);
				const bool rouvert = NkEditeurOuvrir(m);
				std::remove(chemin);
				const bool ancienRelu = rouvert && NbEntites(m.scene) == nE && NbDrapeaux(m.scene) == 0u;
				Temoin(photo && sauve && avecCle && neuve && relu && sansCle && ancienRelu,
					   "(e42) oeil et cadenas : Jouer/Arreter, fichier ; sans eux, relu tel quel", static_cast<float32>(nE));
			}

			// (e43) l'Outliner, gestes rejoues dans un vrai NkGuiContext.
			{
				NkEditeurNouvelleScene(m);
				memory::NkAllocator &alloc = memory::NkGetDefaultAllocator();
				NkEditeurInterface *pui = alloc.New<NkEditeurInterface>(); // gros : sur le tas
				nkgui::NkGuiContext *pctx = alloc.New<nkgui::NkGuiContext>();
				nkgui::NkGuiFont *police = alloc.New<nkgui::NkGuiFont>();
				NkEditeurInterface &ui = *pui;
				nkgui::NkGuiContext &ctx = *pctx;
				const bool policeOk = police->LoadEmbedded(NkEmbeddedFontId::DroidSans, 13.f, false);
				ctx.Init(300, 520);
				ctx.font = policeOk ? police : nullptr;
				const editorkit::NkTheme theme = editorkit::NkTheme::Dark();
				const NkPaletteEditeur pal = NkEditeurPalette(theme);
				NkEditeurCadre c{ctx, m, ui, theme, pal, police, police};
				ui.outliner = nkgui::NkRect{0.f, 0.f, 300.f, 520.f};
				ui.voirOutliner = true;
				ctx.input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				auto trame = [&](float32 dt) {
					ui.dt = dt;
					ctx.BeginFrame(dt);
					ctx.BeginLayout(nkgui::NkRect{0.f, 0.f, 300.f, 520.f});
					ctx.DL().Reset();
					ctx.dlOverlay.Reset();
					NkEditeurDessinerOutliner(c);
					ctx.EndFrame();
				};
				auto touche = [&](nkgui::NkGuiKey k) {
					ctx.input.SetKey(k, true);
					trame(1.f / 60.f);
					ctx.input.SetKey(k, false);
					trame(1.f / 60.f);
				};
				auto taper = [&](const char *s) {
					for (const char *q = s; *q != '\0'; ++q) {
						ctx.input.PushChar(static_cast<uint32>(*q));
					}
					trame(1.f / 60.f);
				};
				auto noeud = [](ecs::NkEntityId e) { return static_cast<nk_uint64>(e.Pack()) + 2u; };
				auto nom = [&](ecs::NkEntityId e) {
					const NkEtiquette *et = m.scene.Monde().Get<NkEtiquette>(e);
					return NkString(et != nullptr ? et->nom : "");
				};
				// Le centre de la ligne de l'entite, a l'ecran : l'en-tete, la
				// recherche et les colonnes font 74 px, une ligne 22 (la racine est
				// la ligne 0).
				auto ligneY = [&](ecs::NkEntityId e) {
					for (uint32 k = 0; k < ui.arbreEntites.Size(); ++k) {
						if (ui.arbreEntites[k] == e) {
							return 74.f + static_cast<float32>(k) * 22.f + 11.f;
						}
					}
					return -100.f;
				};
				auto cliquer = [&](float32 x, float32 y) {
					ctx.input.mousePos = nkgui::NkVec2{x, y};
					ctx.input.mouseDown[0] = true;
					trame(1.f / 60.f);
					ctx.input.mouseDown[0] = false;
					trame(1.f / 60.f);
				};

				const ecs::NkEntityId blob = Par(m.scene, "Blob");
				const ecs::NkEntityId caisse = Par(m.scene, "Caisse");
				trame(1.f / 60.f);

				// (a) F2 : la saisie en place, le nom choisi ; la frappe le REMPLACE.
				m.selection = blob;
				m.aSelection = true;
				const NkString nomBlob = nom(blob);
				NkEditeurExecuter(c, NK_A_RENOMMER);
				trame(1.f / 60.f);
				const bool ouverte = ui.arbre.renaming == noeud(blob) && NkString(ui.arbre.renameBuf) == nomBlob;
				taper("Gros");
				const bool remplace = NkString(ui.arbre.renameBuf) == NkString("Gros");
				touche(nkgui::NkGuiKey::Enter);
				trame(1.f / 60.f);
				const bool ecrit = nom(blob) == NkString("Gros") && ui.arbre.renaming == 0;

				// (b) Echap : rien n'est ecrit.
				m.selection = caisse;
				const NkString nomCaisse = nom(caisse);
				NkEditeurExecuter(c, NK_A_RENOMMER);
				trame(1.f / 60.f);
				taper("Zut");
				touche(nkgui::NkGuiKey::Escape);
				trame(1.f / 60.f);
				const bool abandonne = nom(caisse) == nomCaisse && ui.arbre.renaming == 0;
				Temoin(ouverte && remplace && ecrit && abandonne, "(e43a) F2 : saisie en place, frappe remplace, Entree ecrit, Echap non",
					   static_cast<float32>(ouverte + remplace + ecrit + abandonne));

				// (c) le clic LENT sur le nom de la ligne choisie : la saisie s'ouvre.
				const float32 yCaisse = ligneY(caisse);
				cliquer(200.f, yCaisse);
				for (int32 k = 0; k < 7; ++k) {
					trame(0.1f);
				}
				const bool lent = ui.arbre.renaming == noeud(caisse);
				touche(nkgui::NkGuiKey::Escape);
				// ... le DOUBLE-clic, non : il cadre (contre-epreuve du meme geste, plus vite).
				ui.cadrageDemande = false;
				cliquer(200.f, yCaisse);
				cliquer(200.f, yCaisse);
				for (int32 k = 0; k < 7; ++k) {
					trame(0.1f);
				}
				const bool doubleCadre = ui.arbre.renaming == 0 && ui.cadrageDemande;
				Temoin(lent && doubleCadre, "(e43b) clic lent sur le nom : renommer ; double-clic : cadrer", static_cast<float32>(lent + doubleCadre));

				// (d) l'oeil et le cadenas, a LEUR colonne : apres le chevron, a la
				// profondeur 1 (la regle de NkTreeViewDraw.cpp).
				editorkit::NkTreeViewStyle st;
				st.values = &ui.arbreReglages;
				const float32 xOeil = editorkit::NkTreeMetric(st, "row_pad") + editorkit::NkTreeMetric(st, "indent_step") +
									  editorkit::NkTreeMetric(st, "chevron_w") + editorkit::NkTreeMetric(st, "icon_w") * 0.5f;
				const float32 xCadenas = xOeil + editorkit::NkTreeMetric(st, "icon_w");
				m.selection = blob;
				trame(1.f / 60.f);
				cliquer(xOeil, yCaisse);
				const bool cachee = NkEditeurEstCache(m, caisse) && m.selection == blob; // l'oeil ne choisit pas
				cliquer(xOeil, yCaisse);
				const bool revue = !NkEditeurEstCache(m, caisse);
				cliquer(xCadenas, yCaisse);
				const bool figee = NkEditeurEstVerrouille(m, caisse);
				// Verrouillee, elle se choisit TOUJOURS ici (le cadenas ne fige que la vue).
				cliquer(200.f, yCaisse);
				const bool choisie = m.aSelection && m.selection == caisse;
				cliquer(xCadenas, yCaisse);
				Temoin(cachee && revue && figee && choisie && !NkEditeurEstVerrouille(m, caisse),
					   "(e43c) oeil et cadenas au clic ; verrouillee, choisie quand meme", xOeil);

				alloc.Delete(police);
				alloc.Delete(pctx);
				alloc.Delete(pui);
			}

			// (e44) la hierarchie et les drapeaux.
			{
				NkEditeurNouvelleScene(m);
				m.scene.Camera().PoserZoom(40.f);
				const ecs::NkEntityId pere = NkEditeurCreerEntite(m, "PereD", NkVec2f(0.f, 20.f));
				const ecs::NkEntityId fils = NkEditeurCreerEntite(m, "FilsD", NkVec2f(3.f, 20.f));
				NkSprite2D sp;
				sp.taille = NkVec2f(1.f, 1.f);
				m.scene.Monde().Add<NkSprite2D>(fils, sp);
				const bool rattache = NkEditeurRattacher(m, fils, pere);
				const NkVec2f surFils(3.f, 20.f);
				ecs::NkEntityId e;
				const bool prisAvant = NkEditeurPrendreSous(m, surFils, e) && e == fils;
				NkEditeurCacher(m, pere, true);
				// Le drapeau PROPRE de l'enfant reste faux : c'est l'heritage qui cache.
				const bool cacheParPere = NkEditeurCacheDansLaVue(m, fils) && !NkEditeurEstCache(m, fils) &&
										  !NkEditeurPrendreSous(m, surFils, e);
				NkEditeurCacher(m, pere, false);
				NkEditeurVerrouiller(m, pere, true);
				const bool figeParPere = NkEditeurVerrouilleDansLaVue(m, fils) && !NkEditeurEstVerrouille(m, fils) &&
										 !NkEditeurPrendreSous(m, surFils, e);
				NkEditeurVerrouiller(m, pere, false);
				const bool repris = NkEditeurPrendreSous(m, surFils, e) && e == fils;
				Temoin(rattache && prisAvant && cacheParPere && figeParPere && repris,
					   "(e44) parent cache / verrouille : l'enfant aussi, sans son drapeau", 0.f);
			}

			// (e45) les cartes de l'inspecteur.
			{
				NkEditeurNouvelleScene(m);
				ecs::NkWorld &w45 = m.scene.Monde();
				const ecs::NkEntityId a = Par(m.scene, "Caisse_1");
				const ecs::NkEntityId b = Par(m.scene, "Caisse_2");
				// Reinitialiser : les valeurs, pas l'identite.
				NkSprite2D *sa = w45.Get<NkSprite2D>(a);
				const uint32 tex = sa->texId;
				sa->taille = NkVec2f(3.f, 2.f);
				sa->couche = 7;
				const bool reinit = NkEditeurReinitialiserCarte(m, a, NkCarteEditeur::NK_SPRITE) && sa->taille.x == 1.f && sa->taille.y == 1.f &&
									sa->couche == 0 && sa->texId == tex && tex != 0u;
				// Copier / Coller le corps : la masse part, le corps reste.
				NkCorps2D *ca = w45.Get<NkCorps2D>(a);
				ca->masse = 50.f;
				m.scene.ActualiserCorps(a);
				const NkVec2f posB = w45.Get<NkTransform2D>(b)->position;
				NkPressePapierComposant pp;
				const bool copie = NkEditeurCopierCarte(m, a, NkCarteEditeur::NK_CORPS, pp);
				const bool colle = NkEditeurCollerCarte(m, b, pp);
				const physics::NkRigidBody *rb45 = m.scene.MondePhysique()->GetBody(w45.Get<NkCorps2D>(b)->corpsId);
				// SON corps : distinct de celui de la source, et toujours a SA place.
				// (ActualiserCorps refait le corps : son numero peut changer, pas lui.)
				const bool corpsGarde = rb45 != nullptr && w45.Get<NkCorps2D>(b)->corpsId != w45.Get<NkCorps2D>(a)->corpsId &&
										math::NkAbs(rb45->position.x - posB.x) < 1.0e-4f && math::NkAbs(rb45->position.y - posB.y) < 1.0e-4f;
				const bool solveur = rb45 != nullptr && math::NkAbs(rb45->invMass - 0.02f) < 1.0e-5f;
				// Sur une entite qui n'a pas ce composant : refuse (coller ne l'AJOUTE pas).
				NkPressePapierComposant ps;
				NkEditeurCopierCarte(m, a, NkCarteEditeur::NK_SPRITE, ps);
				const ecs::NkEntityId nue = NkEditeurCreerEntite(m, "Nue45", NkVec2f(0.f, 30.f));
				const bool refuse = ps.carte == static_cast<int32>(NkCarteEditeur::NK_SPRITE) && !NkEditeurCollerCarte(m, nue, ps) &&
									!w45.Has<NkSprite2D>(nue);
				// Le Transform : coller teleporte le corps (le solveur suit).
				NkPressePapierComposant pt;
				w45.Get<NkTransform2D>(a)->rotation = 0.5f;
				m.scene.ActualiserCorps(a);
				NkEditeurCopierCarte(m, a, NkCarteEditeur::NK_TRANSFORM, pt);
				const NkVec2f pa = w45.Get<NkTransform2D>(a)->position;
				const bool colleT = NkEditeurCollerCarte(m, b, pt);
				const physics::NkRigidBody *rbT = m.scene.MondePhysique()->GetBody(w45.Get<NkCorps2D>(b)->corpsId);
				const bool teleporte = colleT && rbT != nullptr && math::NkAbs(rbT->position.x - pa.x) < 1.0e-4f &&
									   math::NkAbs(rbT->position.y - pa.y) < 1.0e-4f && math::NkAbs(w45.Get<NkTransform2D>(b)->rotation - 0.5f) < 1.0e-6f;
				// Monter : la caisse n'a ni matiere, ni source, ni animation, ni
				// animateur ; le Corps (3) monte au-dessus du Collisionneur (2).
				NkEditeurInterface *pui = memory::NkGetDefaultAllocator().New<NkEditeurInterface>();
				nkgui::NkGuiContext *pctx = memory::NkGetDefaultAllocator().New<nkgui::NkGuiContext>();
				const editorkit::NkTheme theme = editorkit::NkTheme::Dark();
				const NkPaletteEditeur pal = NkEditeurPalette(theme);
				NkEditeurCadre c45{*pctx, m, *pui, theme, pal, nullptr, nullptr};
				m.selection = a;
				m.aSelection = true;
				pui->carteMenu = static_cast<int32>(NkCarteEditeur::NK_CORPS);
				NkEditeurDeplacerCarte(c45, -1);
				const bool monte = pui->ordreCartes[2] == static_cast<uint8>(NkCarteEditeur::NK_CORPS) &&
								   pui->ordreCartes[3] == static_cast<uint8>(NkCarteEditeur::NK_COLLISIONNEUR);
				// ... le Sprite (1) ne monte pas au-dessus du Transform (0).
				pui->carteMenu = static_cast<int32>(NkCarteEditeur::NK_SPRITE);
				NkEditeurDeplacerCarte(c45, -1);
				const bool transformEnTete = pui->ordreCartes[0] == 0u && pui->ordreCartes[1] == static_cast<uint8>(NkCarteEditeur::NK_SPRITE);
				memory::NkGetDefaultAllocator().Delete(pctx);
				memory::NkGetDefaultAllocator().Delete(pui);
				Temoin(reinit && copie && colle && corpsGarde && solveur && refuse && teleporte && monte && transformEnTete,
					   "(e45) cartes : reinit, copier/coller (solveur, identite), ordre", rb45 != nullptr ? rb45->invMass : -1.f);
			}

			// (e46) les icones des lumieres, avec la nouvelle prise.
			{
				NkEditeurNouvelleScene(m);
				m.scene.Camera().PoserZoom(40.f);
				const ecs::NkEntityId caisse = Par(m.scene, "Caisse_1");
				const NkVec2f pc = m.scene.Monde().Get<NkTransform2D>(caisse)->position;
				const ecs::NkEntityId lampe = NkEditeurPoserLumiere(m, pc, NkTypeLumiere2D::NK_PONCTUELLE);
				ecs::NkEntityId e;
				const bool icone = NkEditeurPrendreSous(m, pc, e) && e == lampe;
				NkEditeurCacher(m, lampe, true);
				const bool dessous = NkEditeurPrendreSous(m, pc, e) && e == caisse;
				NkEditeurCacher(m, lampe, false);
				Temoin(icone && dessous && !NkEditeurSansVisuel(m, lampe), "(e46) icone de lumiere sur une caisse : l'icone ; cachee : la caisse",
					   static_cast<float32>(icone + dessous));
			}

			// (e47) le Transform d'Unreal et les cases des cartes.
			{
				NkEditeurNouvelleScene(m);
				ecs::NkWorld &w47 = m.scene.Monde();
				const ecs::NkEntityId c = Par(m.scene, "Caisse_1");
				const NkVec2f t0 = w47.Get<NkSprite2D>(c)->taille;
				const NkVec2f d0 = w47.Get<NkCollisionneur2D>(c)->demiTaille;
				const bool pose = NkEditeurPoserEchelle(m, c, NkVec2f(2.f, 2.f));
				const NkVec2f e2 = NkEditeurEchelle(m, c);
				const bool double2 = pose && math::NkAbs(w47.Get<NkSprite2D>(c)->taille.x - t0.x * 2.f) < 1.0e-4f &&
									 math::NkAbs(w47.Get<NkCollisionneur2D>(c)->demiTaille.y - d0.y * 2.f) < 1.0e-4f &&
									 math::NkAbs(e2.x - 2.f) < 1.0e-4f && math::NkAbs(e2.y - 2.f) < 1.0e-4f;
				m.selection = c;
				m.aSelection = true;
				NkEditeurMettreAEchelle(m, NkVec2f(1.5f, 1.5f)); // le gizmo R
				const bool gizmo = math::NkAbs(NkEditeurEchelle(m, c).x - 3.f) < 1.0e-4f;
				NkEditeurPoserEchelle(m, c, NkVec2f(1.f, 1.f));
				const bool rendu = math::NkAbs(w47.Get<NkSprite2D>(c)->taille.x - t0.x) < 1.0e-4f && !w47.Has<NkEchelleEditeur>(c);
				// Les cases.
				const uint32 masque0 = w47.Get<NkCollisionneur2D>(c)->masque;
				NkEditeurActiverCarte(m, c, NkCarteEditeur::NK_COLLISIONNEUR, false);
				const bool colEteint = w47.Get<NkCollisionneur2D>(c)->masque == 0u && w47.Get<NkCollisionneur2D>(c)->couche == 0u &&
									   !NkEditeurCarteActive(m, c, NkCarteEditeur::NK_COLLISIONNEUR);
				NkEditeurActiverCarte(m, c, NkCarteEditeur::NK_COLLISIONNEUR, true);
				const bool colRallume = w47.Get<NkCollisionneur2D>(c)->masque == masque0 && masque0 != 0u;
				NkEditeurActiverCarte(m, c, NkCarteEditeur::NK_CORPS, false);
				const bool corpsEteint = w47.Get<NkCorps2D>(c)->type == NkTypeCorps::NK_CINEMATIQUE && w47.Get<NkCorps2D>(c)->echelleGravite == 0.f;
				// Jouer / Arreter : l'etat eteint (et ce qu'il garde) revient.
				NkEditeurJouer(m);
				NkEditeurAvancer(m, 1.f / 60.f);
				NkEditeurArreter(m);
				const ecs::NkEntityId c2 = Par(m.scene, "Caisse_1");
				const bool traverse = !NkEditeurCarteActive(m, c2, NkCarteEditeur::NK_CORPS);
				NkEditeurActiverCarte(m, c2, NkCarteEditeur::NK_CORPS, true);
				const bool corpsRallume = m.scene.Monde().Get<NkCorps2D>(c2)->type == NkTypeCorps::NK_DYNAMIQUE &&
										  m.scene.Monde().Get<NkCorps2D>(c2)->echelleGravite == 1.f && !m.scene.Monde().Has<NkEteintsEditeur>(c2);
				Temoin(double2 && gizmo && rendu && colEteint && colRallume && corpsEteint && traverse && corpsRallume,
					   "(e47) echelle cuite et relue ; cases : collisionneur, corps, Jouer/Arreter", e2.x);
			}

			memory::NkGetDefaultAllocator().Delete(pm);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC EDITEUR REUSSI" : "BANC EDITEUR EN ECHEC", gR, gE, gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
