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
//   Le moteur PRECOMPILE (2026-10-01, NkEditeurMoteur.h) :
//   (lg7) l'empreinte d'un depot fabrique change avec un octet, un fichier
//         nouveau, la version de Jenga, une variable que lit un .jenga ; elle
//         revient quand l'octet revient ; un .md et le CHEMIN du depot n'y
//         changent rien. (lg7b) celle du vrai depot est stable, et chiffree.
//   (lg8) un cache sans sceau, ou dont une archive manque, ou d'une autre
//         empreinte, n'est pas utilise ; scelle, il l'est, et son chantier
//         est efface ; le verrou est exclusif, et se reprend une fois rendu
//   (lg9) le workspace d'un jeu precompile charge le kit et n'inclut AUCUN
//         module (ni unitest) ; celui des sources les inclut tous, celui du
//         moteur aussi, sans le joueur ; Windows y a droit, Android retombe
//         (ABI), Linux seulement sur demande expresse
//   (lg10) un lien systeme que le joueur lie et que le kit ne transmet pas
//         (d3dcompiler, 30/09) est nomme
//   Le JOURNAL de la construction (2026-10-01, NkEditeurDeroulement.h) :
//   (lg11) une boite d'erreur reelle de Jenga (chemin coupe net sur trois
//         lignes de 92 colonnes) est recousue : fichier, ligne 299, colonne 37,
//         message ; le resume dit « NkJoueurApp.cpp:299: ... » ; aucun cadre
//         ni symbole de Jenga ne reste a l'affichage
//   (lg12) la progression vient des lignes de Jenga : 2 projets, le premier a
//         2/4 fichiers = 25 %, fini = 50 % ; « [2/4] Compiled » reste gris,
//         « Built: » est vert, et le dossier du jeu disparait des chemins
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Livraison/NkEditeurConstruire.h"
#include "Livraison/NkEditeurDeroulement.h"

#include "Editeur/NkEditeurActions.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NKMemory.h"
#include "NKTime/NkChrono.h"
#include "Unkeny/Livraison/NkUnkenyLivraison.h"
#include "Unkeny/Partie/NkUnkenyPartie.h"

#include <cstdio>
#include <cstdlib>

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
				// Le joueur nomme sa liste (_DEPS, 2026-10-01 : elle sert aux deux
				// moteurs, sources et precompile) ; Unkeny la donne en ligne.
				const usize nommee = texte.Find("_DEPS = [");
				const usize debut = nommee != NkString::npos ? nommee : texte.Find("nkentseudependson(");
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
			// (lg1)..(lg6) eprouvent le workspace « sources » ; le precompile
			// (lg7..) est eprouve a part, SANS Jenga : sa preparation lit
			// `jenga --version`, et le banc n'en depend pas.
			d.moteur = NkModeMoteur::NK_SOURCES;
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

			// ── Le moteur PRECOMPILE (2026-10-01), sans Jenga ni compilateur ────
			// (lg7) l'empreinte : celle du CONTENU, sur un depot fabrique
			{
				const NkString fab = sortie + "/depot_fabrique/";
				const NkString ailleurs = sortie + "/ailleurs/depot_copie/";
				NkDirectory::CreateRecursive((fab + "Kernel/NKA/src").CStr());
				NkDirectory::CreateRecursive((fab + "config").CStr());
				NkFile::WriteAllText((fab + "Kernel/NKA/src/a.cpp").CStr(), "int a() { return 1; }\n");
				NkFile::WriteAllText((fab + "Kernel/NKA/NKA.jenga").CStr(), "x = os.getenv(\"NK_BANC_EMPREINTE\", \"\")\n");
				NkFile::WriteAllText((fab + "config/modules.jenga").CStr(), "# registre\n");
				NkVector<NkString> dossiers;
				dossiers.PushBack(NkString("Kernel/NKA"));
				dossiers.PushBack(NkString("config"));
				NkString err;
				NkEmpreinteMoteur e1, e2, e3, e4, e5, e6, e7, e8;
				NkEmpreinteDesDossiers(fab, dossiers, NkString("jenga=2.8.7"), e1, err);
				NkEmpreinteDesDossiers(fab, dossiers, NkString("jenga=2.8.7"), e2, err);
				// Un octet change, non commite : autre empreinte ; remis : la meme.
				NkFile::WriteAllText((fab + "Kernel/NKA/src/a.cpp").CStr(), "int a() { return 2; }\n");
				NkEmpreinteDesDossiers(fab, dossiers, NkString("jenga=2.8.7"), e3, err);
				NkFile::WriteAllText((fab + "Kernel/NKA/src/a.cpp").CStr(), "int a() { return 1; }\n");
				NkEmpreinteDesDossiers(fab, dossiers, NkString("jenga=2.8.7"), e4, err);
				// Un fichier NOUVEAU (non suivi par git) compte ; un .md non.
				NkFile::WriteAllText((fab + "Kernel/NKA/LISEZMOI.md").CStr(), "documentation\n");
				NkEmpreinteDesDossiers(fab, dossiers, NkString("jenga=2.8.7"), e5, err);
				NkFile::WriteAllText((fab + "Kernel/NKA/src/b.cpp").CStr(), "int b() { return 0; }\n");
				NkEmpreinteDesDossiers(fab, dossiers, NkString("jenga=2.8.7"), e6, err);
				NkFile::Delete((fab + "Kernel/NKA/src/b.cpp").CStr());
				// Le meme contenu AILLEURS (un autre worktree) : la meme empreinte.
				NkDirectory::CreateRecursive(ailleurs.CStr());
				NkDirectory::Copy(fab.CStr(), ailleurs.CStr(), true, true);
				NkEmpreinteDesDossiers(ailleurs, dossiers, NkString("jenga=2.8.7"), e7, err);
				// Une autre version de Jenga : autre empreinte.
				NkEmpreinteDesDossiers(fab, dossiers, NkString("jenga=2.8.8"), e8, err);
				// La variable qu'un .jenga lit (VULKAN_SDK...) compte aussi.
				NkEmpreinteMoteur e9;
#if defined(_WIN32)
				_putenv("NK_BANC_EMPREINTE=vulkan");
#else
				setenv("NK_BANC_EMPREINTE", "vulkan", 1);
#endif
				NkEmpreinteDesDossiers(fab, dossiers, NkString("jenga=2.8.7"), e9, err);
#if defined(_WIN32)
				_putenv("NK_BANC_EMPREINTE=");
#else
				unsetenv("NK_BANC_EMPREINTE");
#endif
				const bool contenu = e1.valeur == e2.valeur && e3.valeur != e1.valeur && e4.valeur == e1.valeur;
				const bool fichiers = e5.valeur == e1.valeur && e6.valeur != e1.valeur;
				const bool ailleursMeme = e7.valeur == e1.valeur && !e7.hex.Empty();
				Temoin(contenu && fichiers && ailleursMeme && e8.valeur != e1.valeur && e9.valeur != e1.valeur && e1.fichiers == 3u,
					   "(lg7) empreinte du moteur : contenu, nouveau fichier, Jenga, env ; .md et chemin non",
					   static_cast<float32>(e1.fichiers));
				// Sur le VRAI depot : stable, et chiffre (secondes).
				const float64 t0 = NkChrono::Now().ToSeconds();
				NkEmpreinteMoteur r1, r2;
				const bool lu1 = NkEmpreinteDesDossiers(plan.depot, NkDossiersDuMoteur(), NkString(), r1, err);
				const float64 duree = NkChrono::Now().ToSeconds() - t0;
				const bool lu2 = NkEmpreinteDesDossiers(plan.depot, NkDossiersDuMoteur(), NkString(), r2, err);
				std::printf("         depot : %u fichiers, %.1f Mo, %.2f s, %s\n", static_cast<unsigned>(r1.fichiers),
							static_cast<double>(r1.octets) / (1024.0 * 1024.0), duree, r1.hex.CStr());
				Temoin(lu1 && lu2 && r1.valeur == r2.valeur && r1.fichiers > 500u, "(lg7b) empreinte du vrai depot : stable d'un calcul a l'autre",
					   static_cast<float32>(duree));
			}

			// (lg8) le cache : rien n'est utilise sans SCEAU, et un sceau ne vaut
			//       que si chaque archive est la ; le verrou est exclusif.
			{
				const NkCacheMoteur c = NkCacheDuMoteur(sortie + "/cache", NkString("00000000banc0001"), "Windows", "Debug");
				NkString pourquoi;
				const bool videScelle = NkCacheScelle(c, pourquoi);
				NkDirectory::CreateRecursive((c.dossier + "lib/Debug-Windows").CStr());
				NkDirectory::CreateRecursive(c.chantier.CStr());
				const NkVector<NkString> projets = NkProjetsDuMoteur();
				for (usize i = 0; i < projets.Size(); ++i) {
					NkFile::WriteAllText((c.dossier + "lib/Debug-Windows/" + projets[i] + ".lib").CStr(), "!<arch>\n");
				}
				const NkString joueurTexte = NkFile::ReadAllText((plan.depot + "Applications/UnkenyPlayer/UnkenyPlayer.jenga").CStr());
				NkFile::WriteAllText(c.kit.CStr(), "KIT_SYSTEM_LIBS = {\n    (\"Debug\", \"Windows\"): ['user32', 'gdi32'],\n}\n");
				const bool sansSceau = NkCacheScelle(c, pourquoi);
				NkVector<NkString> details;
				const bool scelle = NkScellerMoteur(c, plan.depot, "Debug", "Windows", NkString("banc"), 1.0, details);
				const bool trouve = NkCacheScelle(c, pourquoi);
				const bool chantierEfface = !NkDirectory::Exists(c.chantier.CStr());
				// Une archive perdue : le kit n'est plus entier.
				NkFile::Delete((c.dossier + "lib/Debug-Windows/" + projets[0] + ".lib").CStr());
				NkString perdu;
				const bool troue = NkCacheScelle(c, perdu);
				// Une AUTRE empreinte ne lit pas ce sceau.
				NkCacheMoteur autre = c;
				autre.empreinte = NkString("00000000banc0002");
				NkString autrePourquoi;
				const bool autreScelle = NkCacheScelle(autre, autrePourquoi);
				NkVerrouMoteur v1, v2;
				const bool pris1 = v1.Prendre(c.verrou);
				const bool pris2 = v2.Prendre(c.verrou);
				v1.Liberer();
				const bool pris3 = v2.Prendre(c.verrou);
				v2.Liberer();
				(void)joueurTexte;
				Temoin(!videScelle && !sansSceau && scelle && trouve && chantierEfface && !troue && perdu.Find("archive") != NkString::npos &&
						   !autreScelle && pris1 && !pris2 && pris3,
					   "(lg8) cache : scelle seulement entier, autre empreinte non ; verrou exclusif", static_cast<float32>(projets.Size()));
			}

			// (lg9) le workspace d'un jeu au moteur precompile : le kit, aucun
			//       module ; le mode sources inchange ; le repli des plateformes
			{
				NkPlanConstruction kit = plan;
				kit.moteur = NkModeMoteur::NK_PRECOMPILE;
				kit.cache = NkCacheDuMoteur(sortie + "/cache", NkString("00000000banc0001"), "Windows", "Debug");
				const NkString texteKit = NkEcrireJengaDuJeu(d, kit);
				const NkString texteSources = NkEcrireJengaDuJeu(d, plan);
				uint32 modulesKit = 0u;
				uint32 modulesSources = 0u;
				for (usize i = 0; i < modules.Size(); ++i) {
					modulesKit += texteKit.Find(modules[i].CStr()) != NkString::npos ? 1u : 0u;
					modulesSources += texteSources.Find(modules[i].CStr()) != NkString::npos ? 1u : 0u;
				}
				const NkString moteurJenga = NkEcrireJengaDuMoteur(plan.depot);
				uint32 modulesMoteur = 0u;
				for (usize i = 0; i < modules.Size(); ++i) {
					modulesMoteur += moteurJenga.Find(modules[i].CStr()) != NkString::npos ? 1u : 0u;
				}
				NkString r;
				const bool windows = NkMoteurPrecompilePossible(NkPlateformeJeu::NK_WINDOWS, false, r);
				const bool android = NkMoteurPrecompilePossible(NkPlateformeJeu::NK_ANDROID, true, r);
				const bool ditAbi = r.Find("ABI") != NkString::npos && r.Find("sources") != NkString::npos;
				const bool linux = NkMoteurPrecompilePossible(NkPlateformeJeu::NK_LINUX, false, r);
				const bool linuxEssai = NkMoteurPrecompilePossible(NkPlateformeJeu::NK_LINUX, true, r);
				Temoin(texteKit.Find("useconfig(MOTEUR + \"/UnkenyMoteur.jenga\")") != NkString::npos &&
						   texteKit.Find("\"moteur\": MOTEUR") != NkString::npos && texteKit.Find("unitest") == NkString::npos &&
						   modulesKit == 0u && modulesSources == modules.Size() && modulesMoteur == modules.Size() &&
						   moteurJenga.Find("UnkenyPlayer") == NkString::npos && windows && !android && ditAbi && !linux && linuxEssai,
					   "(lg9) jeu precompile : le kit, aucun module ; sources inchange ; replis", static_cast<float32>(modulesMoteur));
			}

			// (lg10) les bibliotheques SYSTEME : celles que le joueur lie et que le
			//        kit ne transmet pas sont nommees (d3dcompiler, 30/09)
			{
				const NkString joueur = NkFile::ReadAllText((plan.depot + "Applications/UnkenyPlayer/UnkenyPlayer.jenga").CStr());
				const NkString complet =
					"KIT_SYSTEM_LIBS = {\n    (\"Debug\", \"Windows\"): ['winmm', 'user32', 'gdi32', 'opengl32', 'dwmapi', 'shell32', "
					"'comdlg32', 'mf', 'mfplat', 'mfreadwrite', 'mfuuid', 'uuid', 'ole32', 'dinput8', 'dxguid', 'd3d11', 'd3d12', 'dxgi', "
					"'d3dcompiler', 'avrt'],\n}\n";
				NkString sansD3d = complet;
				const usize k = sansD3d.Find("'d3dcompiler', ");
				sansD3d = NkString(sansD3d.SubStr(0, k)) + NkString(sansD3d.SubStr(k + 15));
				const NkVector<NkString> rien = NkLiensSystemeManquants(complet, joueur, "Debug");
				const NkVector<NkString> un = NkLiensSystemeManquants(sansD3d, joueur, "Debug");
				const NkVector<NkString> lus = NkLiensSystemeDuKit(complet, "Debug", "Windows");
				Temoin(rien.Empty() && un.Size() == 1u && un[0] == NkString("d3dcompiler") && lus.Size() == 20u,
					   "(lg10) liens systeme : un lien du joueur absent du kit est nomme", static_cast<float32>(lus.Size()));
			}

			// ── Le JOURNAL de la construction (2026-10-01), sans Jenga ──────────
			// (lg11) une boite d'erreur de Jenga, telle que construire.log l'a
			//        recue le 01/10 (chemin coupe net a 92 colonnes, sur trois
			//        lignes) : recousue, elle donne fichier, ligne, colonne et
			//        message ; aucun cadre ne reste a l'affichage
			{
				static const char *kBoite[] = {
					"╔══════════════════════════════════════════════════════════════════════════════════════════════╗",
					"║                              Compilation Error: NkJoueurApp.cpp                              ║",
					"╠══════════════════════════════════════════════════════════════════════════════════════════════╣",
					"║ C:\\Users\\rihen\\AppData\\Local\\Temp\\claude\\c--Users-rihen-Documents-Projects-Nkentseu\\47680671 ║",
					"║ -02af-4603-b281-5d1cc24ea730\\scratchpad\\depotcasse\\Applications\\UnkenyPlayer\\src\\Joueur\\NkJo ║",
					"║ ueurApp.cpp:299:37: error: use of undeclared identifier 'graviteDuMonde'                     ║",
					"║   299 |                                 return static_cast<int>(duree * graviteDuMonde);     ║",
					"║       |                                                                 ^~~~~~~~~~~~~~       ║",
					"║ 3 errors generated.                                                                          ║",
					"╚══════════════════════════════════════════════════════════════════════════════════════════════╝",
				};
				NkJournalConstruction j;
				for (const char *l : kBoite) {
					j.LigneJenga(NkString(l), 1.f);
				}
				j.LigneJenga(NkString("\xE2\x9C\x97 \xE2\x9C\x97 Compilation failed: C:\\x\\NkJoueurApp.cpp"), 1.f);
				j.FinCommande(1.f);
				bool cadres = false;
				bool ligneErreur = false;
				bool extrait = false;
				for (usize i = 0; i < j.lignes.Size(); ++i) {
					const NkString &t = j.lignes[i].texte;
					cadres |= t.Find("\xE2\x95") != NkString::npos || t.Find("\xE2\x94") != NkString::npos || t.Find("\xE2\x9C") != NkString::npos;
					ligneErreur |= j.lignes[i].niveau == NkNiveauLigne::NK_ERREUR && t.Find("NkJoueurApp.cpp:299:37: error:") != NkString::npos;
					extrait |= j.lignes[i].niveau == NkNiveauLigne::NK_NOTE && t.Find("299 |") != NkString::npos;
				}
				const bool diag = j.diagnostics.Size() == 1u && j.diagnostics[0].fichier.EndsWith("Joueur\\NkJoueurApp.cpp") &&
								  j.diagnostics[0].fichier.StartsWith("C:\\Users\\") && j.diagnostics[0].ligne == 299 &&
								  j.diagnostics[0].colonne == 37 &&
								  j.diagnostics[0].message == NkString("use of undeclared identifier 'graviteDuMonde'") && j.erreurs == 1;
				const NkVector<NkString> resume = NkResumeErreurs(j, 5u);
				Temoin(diag && !cadres && ligneErreur && extrait && resume.Size() == 1u &&
						   resume[0] == NkString("NkJoueurApp.cpp:299: use of undeclared identifier 'graviteDuMonde'"),
					   "(lg11) boite d'erreur recousue : NkJoueurApp.cpp:299:37, message, sans cadre", static_cast<float32>(j.lignes.Size()));
			}

			// (lg12) la progression vient des lignes de Jenga (projets, fichiers) ;
			//        une ligne « [i/N] Compiled » n'est pas un succes (gris), et
			//        les chemins connus sont raccourcis a l'affichage
			{
				NkJournalConstruction j;
				j.Abreger(NkString("C:/Jeux/Gelee/"), NkString());
				const float32 avant = j.Fraction();
				j.LigneJenga(NkString("Build Order (2 projects):"), 0.f);
				j.LigneJenga(NkString("\xE2\x95\x94\xE2\x95\x90\xE2\x95\x97"), 0.f);
				j.LigneJenga(NkString("\xE2\x95\x91  Project: NKCore                Kind: STATIC_LIB  \xE2\x95\x91"), 0.f);
				j.LigneJenga(NkString("\xE2\x95\x9A\xE2\x95\x90\xE2\x95\x9D"), 0.f);
				j.LigneJenga(NkString("\xE2\x84\xB9 Found 4 source file(s)"), 0.f);
				j.LigneJenga(NkString("\xE2\x9C\x93   [2/4] Compiled: NkCore.cpp"), 0.f);
				const float32 milieu = j.Fraction();
				j.LigneJenga(NkString("\xE2\x9C\x93 Built: C:\\Jeux\\Gelee\\Build\\Lib\\NKCore.lib"), 0.f);
				j.LigneJenga(NkString("\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x90"), 0.f);
				j.LigneJenga(NkString("\xE2\x94\x82  \xE2\x9C\x93 Build Successful        Time: 0.58s  \xE2\x94\x82"), 0.f);
				j.LigneJenga(NkString("\xE2\x94\x94\xE2\x94\x80\xE2\x94\x98"), 0.f);
				const float32 apres = j.Fraction();
				bool compiledGris = false;
				bool construitVert = false;
				bool court = false;
				for (usize i = 0; i < j.lignes.Size(); ++i) {
					const NkLigneJournal &l = j.lignes[i];
					compiledGris |= l.texte == NkString("[2/4] Compiled: NkCore.cpp") && l.niveau == NkNiveauLigne::NK_INFO;
					construitVert |= l.texte.StartsWith("Built: Build\\Lib") && l.niveau == NkNiveauLigne::NK_SUCCES;
					court |= l.texte == NkString("Built: Build\\Lib\\NKCore.lib");
				}
				Temoin(avant < 0.f && milieu > 0.24f && milieu < 0.26f && apres > 0.49f && apres < 0.51f && j.projet == NkString("NKCore") &&
						   compiledGris && construitVert && court,
					   "(lg12) progression lue dans Jenga : 1/2 projet, 2/4 fichiers = 25 % ; chemins courts",
					   milieu * 100.f);
			}

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
