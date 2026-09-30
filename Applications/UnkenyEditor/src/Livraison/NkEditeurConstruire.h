//
// NkEditeurConstruire.h
// =============================================================================
// Description :
//   CONSTRUIRE le jeu pour une plateforme (demande R16 de Rihen, palier U5) :
//   cuire la scene, fabriquer ou copier les icones, ecrire le workspace Jenga
//   du jeu, dire quelles commandes lancer, et ranger le resultat.
//
// Caracteristiques :
//   - Une FONCTION DU MODELE, sans interface (regle de NkEditeurActions.h) :
//     la fenetre « Construire » et la ligne de commande
//     `UnkenyEditor --construire=windows ...` passent par les memes appels, et
//     le banc de l'editeur les eprouve sans fenetre.
//   - Le jeu construit vit dans `<sortie>/<Projet>/` :
//         <Projet>.jenga        le workspace GENERE (voir NkEcrireJengaDuJeu)
//         assets/               les donnees cuites (NkCuireJeu)
//         icones/icone_sombre.png, icones/icone_claire.png   (R17)
//         Build/                le moteur et le jeu, compiles la
//         Livraison/            les paquets (zip, apk...) de l'Expedition
//         construire.log        tout ce que Jenga a ecrit
//   - Le moteur est compile depuis les SOURCES DU DEPOT : sans elles, rien ne
//     se construit, et la fenetre le dit (NkTrouverDepot). Le « kit » Jenga
//     precompile (Kit/2D) est la piste pour s'en passer ; il ne couvre pas
//     encore Unkeny.
//
// ⚠️ LES CHAINES SONT CHERCHEES OU JENGA LES CHERCHE
//   NkDetecterPlateformes refait, en C++, la recherche de Jenga (emsdk, SDK et
//   NDK Android, JDK, SDK OpenHarmony : Jenga/GlobalToolchains.py,
//   Core/Toolchains.py, Core/Builders/*.py). C'est une COPIE, et elle peut
//   diverger : Jenga n'a pas de commande qui dise, lisiblement par un
//   programme, quelles chaines il trouve. La construction reste le juge -- son
//   journal arrive dans l'onglet Journal.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURCONSTRUIRE_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURCONSTRUIRE_H__

#include "Editeur/NkEditeurModele.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editeur {

		/// Pose dans l'environnement de Jenga (JENGA_NO_IDE_CONFIG) : il ne doit
		/// pas semer .vscode/ ni pyrightconfig.json dans le dossier d'un jeu
		/// construit -- c'est pourquoi on y ecrit soi-meme le talon jengaconfig.
		constexpr const char *NK_CONSTRUIRE_SANS_IDE = "1";

		/// Les cibles, dans l'ordre de la demande R16 : Windows d'abord, puis
		/// Android et le Web, puis HarmonyOS, Linux, macOS / iOS.
		enum class NkPlateformeJeu : uint8 {
			NK_WINDOWS = 0,
			NK_ANDROID,
			NK_WEB,
			NK_HARMONYOS,
			NK_LINUX,
			NK_MACOS,
			NK_IOS,
			NK_COUNT
		};
		constexpr int32 NK_NB_PLATEFORMES = static_cast<int32>(NkPlateformeJeu::NK_COUNT);

		const char *NkPlateformeJeuNom(NkPlateformeJeu p) noexcept;	///< « Windows »
		const char *NkPlateformeJeuJenga(NkPlateformeJeu p) noexcept; ///< « windows » (--platform)
		/// « windows », « Android »... (casse indifferente). false si inconnu.
		bool NkPlateformeJeuDepuis(const char *nom, NkPlateformeJeu &sortie) noexcept;

		/// Ce que CETTE machine peut construire, et pourquoi pas sinon.
		struct NkDisponibilite {
				bool disponible = false;
				/// Disponible : la chaine trouvee (« clang-mingw, C:/msys64/... »).
				/// Sinon : ce qui manque, dit en clair.
				NkString raison;
		};
		void NkDetecterPlateformes(NkDisponibilite (&sortie)[NK_NB_PLATEFORMES]);

		/// Developpement = Debug (symboles, sans optimisation) ; Expedition =
		/// Release, et un paquet (zip...) en plus de l'executable.
		enum class NkProfilJeu : uint8 { NK_DEVELOPPEMENT = 0, NK_EXPEDITION };

		struct NkDemandeConstruction {
				NkPlateformeJeu plateforme = NkPlateformeJeu::NK_WINDOWS;
				NkProfilJeu profil = NkProfilJeu::NK_DEVELOPPEMENT;
				/// Le nom du jeu tel qu'on l'affiche (« Gelée ») ; le projet et
				/// l'executable prennent son identifiant (« Gelee »).
				NkString nom;
				/// Les deux versions de l'icone (R17), en PNG. Toutes deux vides :
				/// Unkeny fabrique les siennes. Une seule : refuse (on n'invente
				/// pas la version claire du logo de quelqu'un).
				NkString iconeSombre;
				NkString iconeClaire;
				/// L'executable (et l'APK, le favicon) porte la SOMBRE, la claire
				/// est livree a cote -- comme NKCraft. false : l'inverse.
				bool executableSombre = true;
				/// Le dossier PARENT : le jeu va dans `<sortie>/<Projet>/`.
				NkString sortie;
		};

		/// Une commande a lancer, et ce qu'elle dit d'elle-meme au Journal.
		struct NkEtapeConstruction {
				NkString libelle;
				NkString commande;
		};

		struct NkPlanConstruction {
				NkString depot;		   ///< le depot Nkentseu (barre finale)
				NkString projet;	   ///< l'identifiant du jeu
				NkString dossierJeu;   ///< `<sortie>/<Projet>/`
				NkString fichierJenga; ///< le workspace genere
				NkString journal;	   ///< `<dossierJeu>/construire.log`
				NkVector<NkEtapeConstruction> etapes;
				/// Ce que la construction doit produire (exe, apk, page) et le
				/// paquet de l'Expedition (vide sinon).
				NkString resultat;
				NkString paquet;
				/// Posee par NkAcheverConstruction quand le jeu produit se lance ICI
				/// (Windows construit sous Windows) : `"<exe>" --verifier`. Le jeu
				/// relit ses propres donnees et compare son empreinte a celle de
				/// l'editeur (temoin l1 sur le produit reel). Vide sinon.
				NkString verification;
				uint64 empreinte = 0u; ///< celle de la scene cuite (temoin l1)
		};

		/// L'identifiant d'un nom de jeu : lettres et chiffres ASCII (les
		/// accents francais perdent leur accent), commence par une lettre.
		/// « Ma Gelée 2 » -> « MaGelee2 » ; vide -> « Jeu ».
		NkString NkIdentifiantJeu(const char *nom);

		/// Le depot Nkentseu : `NK_UNKENY_DEPOT` s'il est pose, sinon le premier
		/// dossier, en remontant depuis l'executable, qui contient
		/// config/modules.jenga ET Applications/UnkenyPlayer/UnkenyPlayer.jenga.
		/// Vide si introuvable.
		NkString NkTrouverDepot();

		/// Le dossier de sortie propose : Documents/Unkeny/Jeux.
		NkString NkSortieParDefaut();

		/// Les deux icones fabriquees par Unkeny (512 x 512, PNG) : une gelee sur
		/// fond sombre (#141414) et sur fond clair (#F5F5F5).
		bool NkFabriquerIcones(const char *cheminSombre, const char *cheminClair);

		/// Les modules du moteur qu'inclut le workspace genere (chemins relatifs
		/// au depot), dans l'ordre de Nkentseu.jenga.
		const NkVector<NkString> &NkModulesDuJoueur();

		/// Le texte du workspace genere (expose pour le banc).
		NkString NkEcrireJengaDuJeu(const NkDemandeConstruction &demande, const NkPlanConstruction &plan);

		/// ETAPE 1, sans processus : arrete le jeu s'il tourne (on construit ce
		/// qu'on EDITE, comme on l'enregistre), cuit la scene, pose les icones,
		/// ecrit le workspace, et remplit `plan.etapes`. `vueLargeur` x
		/// `vueHauteur` : le viseur ou la scene est regardee (le joueur montrera
		/// le meme morceau de monde). false = rien a lancer, `journal` dit
		/// pourquoi.
		bool NkPreparerConstruction(NkEditeurModele &m, const NkDemandeConstruction &demande, float32 vueLargeur,
									float32 vueHauteur, NkPlanConstruction &plan, NkVector<NkString> &journal);

		/// ETAPE 3, apres les commandes : range le resultat (sur ordinateur,
		/// les donnees et l'icone livree a cote de l'executable), verifie qu'il
		/// existe (`plan.resultat` devient le chemin reel) et pose
		/// `plan.verification`. false = rien de produit la ou on l'attendait.
		bool NkAcheverConstruction(const NkDemandeConstruction &demande, NkPlanConstruction &plan,
								   NkVector<NkString> &journal);

		/// Une ligne de Jenga, lisible dans le Journal : sans les caracteres
		/// d'encadrement (╔═║…), qui n'existent pas dans la police embarquee.
		/// Vide = la ligne n'etait qu'un cadre.
		NkString NkLigneDeJenga(const NkString &ligne);

		/// `UnkenyEditor --construire=PLATEFORME ...` : la meme construction que
		/// la fenetre, sans fenetre. Voir l'aide (--construire sans valeur).
		/// Rend le code de sortie (0 = construit, et verifie quand on peut).
		int32 NkEditeurConstruireEnLigne(const NkVector<NkString> &args);

		/// Le banc de la construction (lg1..), lance par `--selftest` apres les
		/// autres. Sans Jenga ni compilateur : il prepare, relit et compare.
		/// 0 = tout tient.
		int32 NkEditeurLancerBancLivraison();

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURCONSTRUIRE_H__
