//
// NkEditeurMoteur.h
// =============================================================================
// Description :
//   LE MOTEUR PRECOMPILE d'une construction de jeu : Unkeny et ses modules sont
//   compiles UNE FOIS par (empreinte du moteur, plateforme, configuration) dans
//   un cache partage, hors du dossier du jeu ; chaque jeu ne compile ensuite que
//   son propre code (le joueur) et se LIE aux bibliotheques du cache.
//
// Caracteristiques :
//   - Le cache est un KIT JENGA (`jenga kit`, Jenga 2.8.7 tel quel) : les
//     en-tetes des modules, leurs archives dans l'ordre de lien, les
//     bibliotheques DU SYSTEME qu'ils declarent, et un `UnkenyMoteur.jenga`
//     qui definit `useunkenymoteur()`. Le workspace du jeu le charge par
//     `useconfig` et n'inclut plus AUCUN module du moteur.
//   - Ou il vit (NkRacineCacheMoteur) :
//         %LOCALAPPDATA%/Unkeny/Moteur            (Windows)
//         $XDG_CACHE_HOME/unkeny/moteur ou ~/.cache/unkeny/moteur   (Linux)
//         ~/Library/Caches/Unkeny/Moteur          (macOS)
//     ou NK_UNKENY_CACHE_MOTEUR s'il est pose. LOCAL et non Roaming : ce sont
//     des binaires de CETTE machine et de SES chaines, lourds (70 Mo en Debug),
//     qu'aucun autre poste ne doit recevoir. Hors du depot (on n'y ecrit rien)
//     et hors du jeu (tous les jeux le partagent).
//         <racine>/<empreinte>/<Systeme>-<Config>/          le kit, SCELLE
//         <racine>/<empreinte>/<Systeme>-<Config>/moteur.txt  le sceau
//         <racine>/<empreinte>/chantier-<Systeme>-<Config>/  le workspace du
//                              moteur et ses objets (efface une fois scelle)
//         <racine>/<empreinte>/<Systeme>-<Config>.verrou
//   - L'EMPREINTE (NkEmpreinteDuMoteur) est celle du CONTENU : chaque fichier
//     des dossiers des modules (NkModulesDuJoueur) et de config/, chemin
//     relatif et octets, dans un ordre fixe -- un commit different ou un
//     fichier modifie non commite la change, un depot copie ailleurs (un autre
//     worktree au meme contenu) la garde. S'y ajoutent la version de Jenga, la
//     chaine de la plateforme (chemin, taille et date du compilateur) et les
//     variables d'environnement que les .jenga lisent (VULKAN_SDK, NK_*...).
//     Hors empreinte : les .md (documentation) et les dossiers .git.
//   - Un kit n'est utilise que SCELLE : `moteur.txt` est ecrit EN DERNIER (par
//     renommage), apres verification des archives. Un kit scelle n'est plus
//     jamais reecrit ; celui qui le construit tient le VERROU (NkVerrouMoteur),
//     un fichier ouvert en exclusif que le systeme relache si le processus
//     meurt. Deux constructions en parallele : la seconde attend, puis trouve
//     le kit scelle.
//
// ⚠️ LE MODE « SOURCES » RESTE, EN REPLI
//   `--moteur=sources` (et l'option de la fenetre) garde l'ancienne
//   construction, qui compile le moteur dans le dossier du jeu. Une plateforme
//   que le kit ne sait pas encore servir (NkMoteurPrecompilePossible) y retombe
//   d'elle-meme, et le journal dit pourquoi.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURMOTEUR_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURMOTEUR_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editeur {

		enum class NkPlateformeJeu : uint8;

		/// Le moteur d'une construction : precompile (le cache partage) ou
		/// compile depuis les sources dans le dossier du jeu (l'ancien mode).
		enum class NkModeMoteur : uint8 { NK_PRECOMPILE = 0, NK_SOURCES };

		const char *NkModeMoteurNom(NkModeMoteur m) noexcept; ///< « precompile », « sources »
		/// « precompile » / « sources » (casse indifferente). false si inconnu.
		bool NkModeMoteurDepuis(const char *nom, NkModeMoteur &sortie) noexcept;

		/// La plateforme peut-elle se lier a un moteur precompile ? `raison` dit
		/// toujours pourquoi (oui ou non). `essai` : demande EXPLICITE
		/// (`--moteur=precompile`) -- une plateforme possible mais pas encore
		/// eprouvee (Linux, macOS, Web) est alors tentee ; par defaut elle
		/// retombe sur les sources.
		bool NkMoteurPrecompilePossible(NkPlateformeJeu p, bool essai, NkString &raison);

		/// La racine du cache (voir l'en-tete), barre finale comprise.
		NkString NkRacineCacheMoteur();

		/// « 2.8.7 » : `jenga --version`, lu une fois par processus. Vide si
		/// Jenga ne repond pas.
		NkString NkVersionJenga();

		/// La signature de la chaine qui construit pour `p` : ce que dit
		/// NkDetecterPlateformes, plus la taille et la date du compilateur quand
		/// c'est un fichier (une mise a jour de clang change l'empreinte).
		NkString NkSignatureChaine(NkPlateformeJeu p);

		struct NkEmpreinteMoteur {
				uint64 valeur = 0u;
				NkString hex; ///< 16 chiffres hexadecimaux
				uint32 fichiers = 0u;
				uint64 octets = 0u;
				NkString environnement; ///< « VULKAN_SDK= ; NK_ENABLE_VULKAN=... » lues
		};

		/// Les dossiers (relatifs au depot) dont le CONTENU fait l'empreinte :
		/// celui de chaque module de NkModulesDuJoueur, et config/.
		NkVector<NkString> NkDossiersDuMoteur();

		/// L'empreinte des `dossiers` de `depot`, plus `extra` (Jenga, chaine...).
		/// Exposee pour le banc, qui l'eprouve sur un depot fabrique. false si
		/// un dossier manque (`erreur` le nomme).
		bool NkEmpreinteDesDossiers(const NkString &depot, const NkVector<NkString> &dossiers, const NkString &extra,
									NkEmpreinteMoteur &e, NkString &erreur);

		/// L'empreinte du moteur de `depot` pour la plateforme `p`.
		bool NkEmpreinteDuMoteur(const NkString &depot, NkPlateformeJeu p, NkEmpreinteMoteur &e, NkString &erreur);

		/// Le cache d'un moteur pour une (plateforme, configuration).
		struct NkCacheMoteur {
				NkString racine;	///< <racine>/<empreinte>/
				NkString dossier;	///< le kit : <racine>/<empreinte>/<Systeme>-<Config>/
				NkString kit;		///< dossier + UnkenyMoteur.jenga
				NkString sceau;		///< dossier + moteur.txt
				NkString chantier;	///< le workspace du moteur, ses objets
				NkString verrou;	///< le fichier du verrou
				NkString empreinte; ///< hex
				NkString systeme;	///< « Windows » (%{cfg.system} de Jenga)
				NkString config;	///< « Debug »
		};
		NkCacheMoteur NkCacheDuMoteur(const NkString &racine, const NkString &empreinte, const char *systeme,
									  const char *config);

		/// Le kit est-il SCELLE et entier (sceau a la bonne empreinte, fichier
		/// du kit, chaque archive citee par le sceau) ? `pourquoi` dit sinon ce
		/// qui manque.
		bool NkCacheScelle(const NkCacheMoteur &c, NkString &pourquoi);

		/// Le nom des projets des modules (NkModulesDuJoueur sans chemin ni
		/// extension : NKCore, Unkeny...), dans l'ordre.
		NkVector<NkString> NkProjetsDuMoteur();

		/// Les bibliotheques du systeme qu'un kit transmet pour (config,
		/// systeme), lues dans le texte de son .jenga (KIT_SYSTEM_LIBS).
		NkVector<NkString> NkLiensSystemeDuKit(const NkString &texteKit, const char *config, const char *systeme);

		/// Les bibliotheques Windows que le joueur lie lui-meme (_WIN_LINKS de
		/// UnkenyPlayer.jenga) et que le kit ne transmet PAS : un module qui
		/// s'en sert sans la declarer (d3dcompiler, 30/09). `vulkan-1` n'est
		/// pas compte (optionnel).
		NkVector<NkString> NkLiensSystemeManquants(const NkString &texteKit, const NkString &texteJoueur, const char *config);

		/// Scelle le kit apres `jenga kit` : verifie les archives, ecrit le sceau
		/// (par renommage, en dernier), efface le chantier. `details` : ce qui
		/// va au journal. false = le kit est incomplet (rien n'est scelle).
		bool NkScellerMoteur(const NkCacheMoteur &c, const NkString &depot, const char *config, const char *systeme,
							 const NkString &versionJenga, float64 duree, NkVector<NkString> &details);

		/// Le verrou d'un cache : un fichier ouvert en EXCLUSIF (Windows :
		/// partage refuse ; ailleurs : flock). Le systeme le relache si le
		/// processus meurt : pas de verrou orphelin. Heritable par les
		/// programmes lances pendant qu'il est tenu (jenga) : un jenga orphelin
		/// le garde, et personne ne vient ecrire dans son chantier.
		class NkVerrouMoteur {
			public:
				NkVerrouMoteur() = default;
				~NkVerrouMoteur();
				NkVerrouMoteur(const NkVerrouMoteur &) = delete;
				NkVerrouMoteur &operator=(const NkVerrouMoteur &) = delete;

				/// Non bloquant : true = tenu (ou deja tenu par nous).
				bool Prendre(const NkString &chemin);
				void Liberer();
				bool Tenu() const noexcept {
					return mTenu;
				}

			private:
				void *mPoignee = nullptr;
				int32 mFd = -1;
				bool mTenu = false;
		};

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURMOTEUR_H__
