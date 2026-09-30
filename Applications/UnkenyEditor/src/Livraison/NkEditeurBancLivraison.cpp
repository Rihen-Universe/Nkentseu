//
// NkEditeurBancLivraison.cpp
// =============================================================================
// Description :
//   `UnkenyEditor --selftest`, suite : la CONSTRUCTION, sans Jenga ni
//   compilateur. On prepare comme la fenetre « Construire », on relit comme le
//   joueur autonome, et on compare.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (lg1) la scene NEUVE de l'editeur (sol, murs, caisses texturees, blob,
//         gelee, ballon, eau, tissu), preparee par NkPreparerConstruction puis
//         relue par NkChargerJeu (le code du joueur), a la MEME EMPREINTE que
//         dans l'editeur ; rien ne manque
//   (lg1n) une scene modifiee APRES la cuisson n'a plus l'empreinte cuite
//   (lg2) le workspace genere inclut le joueur (UNKENY_JEU, UnkenyPlayer.jenga)
//         et chaque module qu'il cite existe dans le depot ; chaque dependance
//         de UnkenyPlayer.jenga y a son module (sinon Jenga la retirerait sans
//         bruit et l'edition de liens tomberait)
//   (lg3) R17 : les deux icones sont posees, et elles DIFFERENT ; une seule
//         icone donnee est refusee, et le journal dit pourquoi
//   (lg4) chaque plateforme a une raison ecrite (disponible ou non) : aucune
//         n'est grisee en silence
//   (lg5) l'identifiant d'un nom : « Ma Gelée 2 » -> MaGelee2, vide -> Jeu,
//         « 3D » -> Jeu3D
//   (lg6) jouer 1 s dans l'EDITEUR (NkEditeurJouer + NkEditeurAvancer) et dans
//         le JOUEUR (NkAvancerPartie) depuis le meme jeu cuit : meme empreinte
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Livraison/NkEditeurConstruire.h"

#include "Editeur/NkEditeurActions.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Livraison/NkUnkenyLivraison.h"
#include "Unkeny/Partie/NkUnkenyPartie.h"

#include <cstdio>

namespace nkentseu {
	namespace editeur {

		namespace {
			int32 gE = 0;
			int32 gR = 0;

			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			struct NkRelu {
					unkeny::NkScene scene;
					unkeny::NkTextures2D textures;
					unkeny::NkJeuCharge jeu;
			};

			/// Les noms de modules de `nkentseudependson([...])` dans le .jenga
			/// du joueur : les chaines entre guillemets du premier crochet.
			NkVector<NkString> DependancesDuJoueur(const NkString &texte) {
				NkVector<NkString> noms;
				const usize debut = texte.Find("nkentseudependson(");
				if (debut == NkString::npos) {
					return noms;
				}
				const usize ouvre = texte.Find('[', debut);
				const usize ferme = ouvre == NkString::npos ? NkString::npos : texte.Find(']', ouvre);
				if (ferme == NkString::npos) {
					return noms;
				}
				NkString courant;
				bool dedans = false;
				for (usize i = ouvre; i < ferme; ++i) {
					const char c = texte[i];
					if (c == '"') {
						if (dedans) {
							noms.PushBack(courant);
							courant = NkString();
						}
						dedans = !dedans;
						continue;
					}
					if (dedans) {
						courant.Append(c);
					}
				}
				return noms;
			}
		} // namespace

		int32 NkEditeurLancerBancLivraison() {
			gE = 0;
			gR = 0;
			std::printf("\nUnkenyEditor — banc de la construction (preparer, relire, comparer)\n\n");
			auto &tas = memory::NkGetDefaultAllocator();
			const NkString sortie = (NkDirectory::GetTempDirectory() / "unkeny_banc_construire").ToString();
			NkDirectory::Delete(sortie.CStr(), true);

			NkEditeurModele *pm = tas.New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);
			const uint64 empreinteEditeur = unkeny::NkEmpreinteScene(m.scene, &m.textures);

			NkDemandeConstruction d;
			d.nom = NkString("Banc Gelée");
			d.sortie = sortie;
			NkPlanConstruction plan;
			NkVector<NkString> journal;
			const bool pret = NkPreparerConstruction(m, d, 800.f, 450.f, plan, journal);

			// (lg1)
			NkRelu *relu = tas.New<NkRelu>();
			const bool jouable = unkeny::NkChargerJeu((plan.dossierJeu + "assets/").CStr(), relu->scene, relu->textures, nullptr, relu->jeu);
			Temoin(pret && jouable && plan.empreinte == empreinteEditeur && relu->jeu.empreinteLue == empreinteEditeur &&
					   relu->jeu.manquantes.Empty() && relu->jeu.textures >= 1u,
				   "(lg1) scene de l'editeur preparee puis relue : meme empreinte", static_cast<float32>(relu->jeu.textures));

			// (lg1n)
			NkPoserActeurSim(m.scene, NkActeurSim::NK_CAISSE, NkVec2f(0.f, 2.f), &m.ressources);
			Temoin(unkeny::NkEmpreinteScene(m.scene, &m.textures) != plan.empreinte,
				   "(lg1n) une caisse posee apres la cuisson : l'empreinte change", 0.f);

			// (lg2)
			const NkString jenga = NkFile::ReadAllText(plan.fichierJenga.CStr());
			const NkVector<NkString> &modules = NkModulesDuJoueur();
			uint32 presents = 0u;
			for (usize i = 0; i < modules.Size(); ++i) {
				const bool cite = jenga.Find(modules[i].CStr()) != NkString::npos;
				presents += (cite && NkFile::Exists((plan.depot + modules[i]).CStr())) ? 1u : 0u;
			}
			const NkString joueur = NkFile::ReadAllText((plan.depot + "Applications/UnkenyPlayer/UnkenyPlayer.jenga").CStr());
			NkVector<NkString> deps = DependancesDuJoueur(joueur);
			// ... ET celles d'Unkeny, que le joueur tire sans les nommer : c'est
			// par la qu'est arrive NKAnima (30/09), invisible au seul joueur.
			const NkString moteur = NkFile::ReadAllText((plan.depot + "Engine/Unkeny/Unkeny.jenga").CStr());
			const NkVector<NkString> depsMoteur = DependancesDuJoueur(moteur);
			for (usize i = 0; i < depsMoteur.Size(); ++i) {
				deps.PushBack(depsMoteur[i]);
			}
			uint32 couvertes = 0u;
			for (usize i = 0; i < deps.Size(); ++i) {
				const NkString fichier = NkString("/") + deps[i] + ".jenga";
				for (usize k = 0; k < modules.Size(); ++k) {
					if (modules[k].EndsWith(fichier.CStr())) {
						++couvertes;
						break;
					}
				}
			}
			Temoin(presents == modules.Size() && !deps.Empty() && couvertes == deps.Size() &&
					   jenga.Find("UNKENY_JEU") != NkString::npos && jenga.Find("Applications/UnkenyPlayer/UnkenyPlayer.jenga") != NkString::npos,
				   "(lg2) workspace genere : le joueur, et tous ses modules", static_cast<float32>(couvertes));

			// (lg3)
			const NkVector<nk_uint8> sombre = NkFile::ReadAllBytes((plan.dossierJeu + "icones/icone_sombre.png").CStr());
			const NkVector<nk_uint8> claire = NkFile::ReadAllBytes((plan.dossierJeu + "icones/icone_claire.png").CStr());
			bool differentes = sombre.Size() != claire.Size();
			for (usize i = 0; !differentes && i < sombre.Size(); ++i) {
				differentes = sombre[i] != claire[i];
			}
			NkDemandeConstruction une = d;
			une.iconeSombre = plan.dossierJeu + "icones/icone_sombre.png";
			NkPlanConstruction planUne;
			NkVector<NkString> journalUne;
			const bool refuse = !NkPreparerConstruction(m, une, 800.f, 450.f, planUne, journalUne);
			const bool ditR17 = !journalUne.Empty() && journalUne[journalUne.Size() - 1u].Find("R17") != NkString::npos;
			Temoin(sombre.Size() > 100u && claire.Size() > 100u && differentes && refuse && ditR17,
				   "(lg3) R17 : icones sombre ET claire, differentes ; une seule : refuse", static_cast<float32>(sombre.Size()));

			// (lg4)
			NkDisponibilite dispo[NK_NB_PLATEFORMES];
			NkDetecterPlateformes(dispo);
			int32 dites = 0;
			for (int32 i = 0; i < NK_NB_PLATEFORMES; ++i) {
				dites += dispo[i].raison.Empty() ? 0 : 1;
			}
			Temoin(dites == NK_NB_PLATEFORMES, "(lg4) chaque plateforme a sa raison ecrite (aucune grisee en silence)",
				   static_cast<float32>(dites));

			// (lg5)
			const bool ids = NkIdentifiantJeu("Ma Gelée 2") == NkString("MaGelee2") && NkIdentifiantJeu("") == NkString("Jeu") &&
							 NkIdentifiantJeu("3D") == NkString("Jeu3D");
			Temoin(ids, "(lg5) identifiant : MaGelee2, Jeu, Jeu3D", 0.f);

			// (lg6) l'editeur et le joueur jouent la MEME partie
			NkRelu *partie = tas.New<NkRelu>();
			unkeny::NkChargerJeu((plan.dossierJeu + "assets/").CStr(), partie->scene, partie->textures, nullptr, partie->jeu);
			m.chemin = plan.dossierJeu + "assets/scene.nkscene";
			const bool ouvert = NkEditeurOuvrir(m);
			NkEditeurJouer(m);
			for (int32 k = 0; k < 60; ++k) {
				NkEditeurAvancer(m, 1.f / 60.f);
				unkeny::NkAvancerPartie(partie->scene, 1.f / 60.f);
			}
			const uint64 eEditeur = unkeny::NkEmpreinteScene(m.scene, &m.textures);
			const uint64 eJoueur = unkeny::NkEmpreinteScene(partie->scene, &partie->textures);
			Temoin(ouvert && eEditeur == eJoueur && eJoueur != plan.empreinte,
				   "(lg6) 1 s jouee dans l'editeur et dans le joueur : meme empreinte", 60.f);

			tas.Delete(partie);
			tas.Delete(relu);
			tas.Delete(pm);
			NkDirectory::Delete(sortie.CStr(), true);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC CONSTRUCTION REUSSI" : "BANC CONSTRUCTION EN ECHEC", gR, gE,
						gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
