// =============================================================================
// NkJoueurBancEntrees.cpp — le banc des ENTREES du joueur autonome (30/09)
//
// Lance par `UnkenyPlayer --selftest`, apres le banc de la livraison. Sans
// fenetre : les evenements sont construits ici et passent par les MEMES
// fonctions que NkJoueurApp (NkJoueurEntreesEvenement, NkJoueurEntreesTrame,
// NkJoueurEntreesBrancher), et la scene avance par NkAvancerPartie, comme en
// jeu. Aucune entree globale.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (j1)  le niveau du jalon « Gelee » (Unkeny/Jeu/NkUnkenyNiveauGelee.h) : le
//         heros pose, Espace fait accorder UN saut par son controleur, et il
//         monte d'au moins 30 cm ; (j1n) sans Espace : aucun saut, il ne monte
//         pas de 10 cm
//   (j2)  P est l'action Pause : UNE bascule a l'appui, aucune tant qu'il est
//         tenu ; Echap est a la coquille (QUITTER) et ne touche aucune action
//   (j3)  les entrees CUITES : « lier Sauter Key:K » remplace les standard --
//         K fait Sauter, Espace ne fait plus rien ; une ligne d'action inconnue
//         est refusee avec son numero (ligne 2), les autres s'appliquent ; un
//         texte vide rend les liaisons standard
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Joueur/NkJoueurEntrees.h"

#include "NKEvent/NkKeyboardEvent.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Jeu/NkUnkenyNiveauGelee.h"
#include "Unkeny/Partie/NkUnkenyPartie.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

#include <cstdio>

namespace nkentseu {
	namespace joueur {

		namespace {
			int32 gE = 0;
			int32 gR = 0;

			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			/// Une partie du jalon : `espace` = le joueur appuie sur Espace une fois
			/// le heros pose. Rend la montee (m) et le nombre de sauts accordes.
			void Partie(bool espace, float32 &montee, uint32 &sauts, bool &brancheUneFois) {
				memory::NkAllocator &alloc = memory::NkGetDefaultAllocator();
				unkeny::NkScene *ps = alloc.New<unkeny::NkScene>();
				NkJoueurEntrees *pe = alloc.New<NkJoueurEntrees>();
				unkeny::NkScene &s = *ps;
				NkJoueurEntrees &e = *pe;
				unkeny::NkSceneConfig cfg;
				cfg.physique = true;
				cfg.particules = true;
				cfg.gravite = math::NkVec2f(0.f, -9.81f);
				s.Init(cfg);
				unkeny::NkNiveauGelee n;
				unkeny::NkConstruireNiveauGelee(s, n, true);
				brancheUneFois = NkJoueurEntreesBrancher(e, s) != 0u;
				const float32 dt = 1.f / 60.f;
				for (int32 i = 0; i < 40; ++i) { // qu'il se pose
					NkJoueurEntreesTrame(e, nullptr, dt);
					unkeny::NkAvancerPartie(s, dt);
				}
				const float32 y0 = unkeny::NkPositionHerosGelee(s, n).y;
				if (espace) {
					const NkKeyPressEvent appui(NkKey::NK_SPACE);
					brancheUneFois = brancheUneFois && NkJoueurEntreesEvenement(e, appui) == NkDemandeJoueur::NK_RIEN;
				}
				float32 yMax = y0;
				for (int32 i = 0; i < 24; ++i) {
					NkJoueurEntreesTrame(e, nullptr, dt);
					unkeny::NkAvancerPartie(s, dt);
					yMax = math::NkMax(yMax, unkeny::NkPositionHerosGelee(s, n).y);
				}
				const unkeny::NkEtatControle2D *c = unkeny::NkEtatHerosGelee(s, n);
				sauts = c != nullptr ? c->sauts : 999u;
				montee = yMax - y0;
				alloc.Delete(pe);
				alloc.Delete(ps);
			}
		} // namespace

		int32 NkJoueurLancerBancEntrees() {
			gE = 0;
			gR = 0;
			std::printf("\nUnkenyPlayer — banc des ENTREES du joueur (actions standard, jalon Gelee)\n\n");
			memory::NkAllocator &alloc = memory::NkGetDefaultAllocator();

			// ── (j1) ──
			{
				float32 monte = 0.f;
				float32 monteSans = 0.f;
				uint32 sauts = 0u;
				uint32 sautsSans = 0u;
				bool ok = false;
				bool okSans = false;
				Partie(true, monte, sauts, ok);
				Partie(false, monteSans, sautsSans, okSans);
				std::printf("        Espace : %u saut(s), montee %.2f m ; sans : %u saut(s), %.2f m\n", sauts, static_cast<double>(monte),
							sautsSans, static_cast<double>(monteSans));
				Temoin(ok && sauts == 1u && monte >= 0.3f, "(j1) Espace fait sauter le heros de Gelee (montee m)", monte);
				Temoin(okSans && sautsSans == 0u && monteSans < 0.1f, "(j1n) sans Espace : aucun saut, il ne monte pas (m)", monteSans);
			}

			// ── (j2) ──
			{
				NkJoueurEntrees *pe = alloc.New<NkJoueurEntrees>();
				NkJoueurEntrees &e = *pe;
				const NkKeyPressEvent p(NkKey::NK_P);
				const NkKeyReleaseEvent pLache(NkKey::NK_P);
				const bool coquilleNeLaPrendPas = NkJoueurEntreesEvenement(e, p) == NkDemandeJoueur::NK_RIEN;
				const bool bascule = NkJoueurEntreesTrame(e) == NkDemandeJoueur::NK_BASCULER_PAUSE;
				const bool tenue = NkJoueurEntreesTrame(e) == NkDemandeJoueur::NK_RIEN;
				NkJoueurEntreesEvenement(e, pLache);
				NkJoueurEntreesTrame(e);
				const NkKeyPressEvent echap(NkKey::NK_ESCAPE);
				const bool quitte = NkJoueurEntreesEvenement(e, echap) == NkDemandeJoueur::NK_QUITTER;
				NkJoueurEntreesTrame(e);
				bool rien = true;
				for (int32 a = 0; a < unkeny::NK_ACTIONS_STANDARD; ++a) {
					rien = rien && e.Actions().Valeur(a) == 0.f;
				}
				Temoin(coquilleNeLaPrendPas && bascule && tenue && quitte && rien,
					   "(j2) P : UNE bascule de pause ; Echap quitte sans toucher au jeu", 0.f);
				alloc.Delete(pe);
			}

			// ── (j3) ──
			{
				NkJoueurEntrees *pe = alloc.New<NkJoueurEntrees>();
				NkJoueurEntrees &e = *pe;
				const unkeny::NkRapportLiaisons r = NkJoueurEntreesLire(e, NkString("lier Sauter Key:K\nlier Voler Key:V\n"));
				const bool refus = r.erreurs.Size() == 1u && r.erreurs[0].Find("ligne 2") != NkString::npos && r.appliquees == 1u;
				if (!r.erreurs.Empty()) {
					std::printf("        refus : %s\n", r.erreurs[0].CStr());
				}
				const NkKeyPressEvent k(NkKey::NK_K);
				const NkKeyReleaseEvent kLache(NkKey::NK_K);
				const NkKeyPressEvent espace(NkKey::NK_SPACE);
				const NkKeyReleaseEvent espaceLache(NkKey::NK_SPACE);
				NkJoueurEntreesEvenement(e, k);
				NkJoueurEntreesTrame(e);
				const bool kSaute = e.Actions().Enfoncee(NK_JOUEUR_SAUTER);
				NkJoueurEntreesEvenement(e, kLache);
				NkJoueurEntreesEvenement(e, espace);
				NkJoueurEntreesTrame(e);
				const bool espaceMuet = !e.Actions().Enfoncee(NK_JOUEUR_SAUTER);
				NkJoueurEntreesEvenement(e, espaceLache);
				NkJoueurEntreesTrame(e);
				NkJoueurEntreesLire(e, NkString());
				NkJoueurEntreesEvenement(e, espace);
				NkJoueurEntreesTrame(e);
				const bool standard = e.Actions().Enfoncee(NK_JOUEUR_SAUTER);
				Temoin(refus && kSaute && espaceMuet && standard,
					   "(j3) entrees cuites : K fait Sauter, Espace non ; ligne 2 refusee ; vide = standard", 0.f);
				alloc.Delete(pe);
			}

			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC ENTREES DU JOUEUR REUSSI" : "BANC ENTREES DU JOUEUR EN ECHEC", gR, gE,
						gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace joueur
} // namespace nkentseu
