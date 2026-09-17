#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverse.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  NKConverse — PARLER A UN MODELE, ET RIEN D'AUTRE
// =============================================================================
//  Ce module ne sait pas ce qu'est un document, un maillage ni une scene. Il
//  transporte une INVITE vers un modele et en rapporte une REPONSE, ou un
//  REFUS NOMME. C'est tout son contrat, et sa pauvrete est le point : chaque
//  capacite propre a UN moteur (jetons, streaming, fenetre de contexte,
//  temperature, outils) deviendrait la porte par laquelle le remplacement
//  serait impossible.
//
//  POURQUOI IL EXISTE : le meme patron etait ecrit TROIS FOIS dans la maison
//  — PV3DE (`NkIConvBackend`, le patron d'origine), NKUIDesign
//  (`NkIDesignBackend`, dont ce module est extrait) et NKCode (`NkAiPanel`).
//  Deux listes ecrites separement finissent toujours par diverger, et
//  celles-ci avaient deja commence.
//
//  LES TROIS REGLES DE CE MODULE
//  -----------------------------
//  1. IL NE DEPEND JAMAIS DE Kernel/AI NI DE Runtime. Le jour ou quelqu'un a
//     besoin de l'inverse, c'est le CONSOMMATEUR qui compose, pas le module
//     qui monte d'une couche. Mesure qui l'impose : `Kernel/AI/NKAgent`
//     declare NKTensor, NKRL et NKInfer — y loger la conversation aurait
//     fait tirer tout le moteur d'inference dans NK3DModeler, qui n'a
//     besoin que de lire un fichier.
//  2. NKThreading ET NKTime SONT DECLARES dans `NKConverse.jenga`, et un
//     en-tete inclut ce qu'il utilise. C'est la faute exacte qui a casse
//     NKPA avec 20 erreurs : un chronometre sans son include compilait chez
//     deux consommateurs par chance, et par chance seulement.
//  3. LE CONTRAT RESTE PAUVRE : une invite, une reponse, un refus nomme.
//     Tout ce qui sait ce qu'est un document, un maillage ou une scene reste
//     chez le consommateur.
//
//  ⚠️ OU PASSE LA COUPE, ET POURQUOI PAS LA OU ON CROYAIT. La coupe annoncee
//     etait la ligne 429 de `DesignAI.h` (le debut de l'orchestrateur). La
//     lecture la remonte a 383 : `NkAIVerdict` a UN seul membre neutre
//     (`BackendMuet`) sur six — les autres disent « pas de document lisible »,
//     « composant que le REGISTRE ignore », « ne se rejoue pas », « cible de
//     GREFFE invalide » — et `NkAIResult` porte `graftedRoot`, `nodesAdded`,
//     `unknownComponents`, `replayDiffs`. Ces types COMPILENT sans document,
//     mais ils en parlent : les descendre aurait fait entrer le vocabulaire
//     du design dans un module qui doit l'ignorer. Ils restent chez leur
//     proprietaire.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkFile.h"
#include "NKCore/NkTypes.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
	#include <windows.h>
#endif

namespace nkentseu::converse {


	// ── LA REQUETE ET LA REPONSE ────────────────────────────────────────────
	struct NkConverseRequest {
			NkString prompt;	 ///< ce que l'utilisateur demande, en francais
			NkString catalog;	 ///< les composants DECLARES, engendres depuis le registre
			NkString currentDoc; ///< le document courant, pour qu'elle puisse le CONTINUER
	};

	struct NkConverseReply {
			NkString text; ///< la reponse brute du backend
			bool success = false;
			NkString error;
	};

	// ── L'INTERFACE DE BACKEND ──────────────────────────────────────────────
	// Meme forme que `NkIConvBackend` de PV3DE, volontairement : c'est le patron
	// qui a deja servi, et un second patron pour la meme chose serait une
	// divergence gratuite.
	class NkIConverseBackend {
		public:
			virtual ~NkIConverseBackend() = default;
			virtual bool Complete(const NkConverseRequest &req, NkConverseReply &out) = 0;
			virtual bool IsAvailable() const = 0;
			virtual const char *Name() const = 0;
	};

	// ── BACKEND FICHIER ─────────────────────────────────────────────────────
	// Il ecrit le prompt complet dans un fichier et lit la reponse dans un autre.
	//
	// ⚠️ CE N'EST PAS UN BOUCHON. C'est le seul backend qui fonctionne
	//    AUJOURD'HUI, sans reseau, avec n'importe quel modele : on donne le
	//    fichier de prompt a qui on veut, on colle la reponse dans le fichier de
	//    reponse, l'outil la verifie et la pose. Le jour ou un backend reseau
	//    arrive, celui-ci reste utile — c'est lui qui permet de rejouer une
	//    reponse a l'identique pour comprendre un rejet.
	class NkConverseBackendFichier final : public NkIConverseBackend {
		public:
			NkString promptPath = NkString("nkuidesign_prompt.txt");
			NkString replyPath = NkString("nkuidesign_reponse.txt");

			bool Complete(const NkConverseRequest &req, NkConverseReply &out) override {
				NkString full;
				full.Append(req.prompt);
				full.Append("\n\n--- composants declares ---\n");
				full.Append(req.catalog);
				full.Append("\n--- document courant ---\n");
				full.Append(req.currentDoc);
				nkentseu::NkFile::WriteAllText(promptPath.Data(), full.Data());

				if (!nkentseu::NkFile::Exists(replyPath.Data())) {
					out.success = false;
					out.error = NkString("Prompt ecrit dans ");
					out.error.Append(promptPath);
					out.error.Append(" — collez la reponse dans ");
					out.error.Append(replyPath);
					return false;
				}
				out.text = nkentseu::NkFile::ReadAllText(replyPath.Data());
				out.success = out.text.Length() > 0;
				if (!out.success)
					out.error = NkString("Fichier de reponse vide.");
				return out.success;
			}
			bool IsAvailable() const override {
				return true;
			}
			const char *Name() const override {
				return "fichier";
			}
	};

	// -- BACKEND PAR PROCESSUS EXTERNE ---------------------------------------
	// ATTENTION : C'EST LA MEME FORME QUE LE PONT 3D DU MODELEUR, ET C'EST VOULU.
	//    `NK3DModeler/Genia/NkGenerateur.h` ne connait ni TripoSR ni PyTorch :
	//    il connait un GABARIT de ligne de commande a deux trous, `{image}` et
	//    `{out}`, et une regle -- « code 0 et le fichier existe, ou un refus
	//    nomme ». Le jour ou un modele entraine chez Rihen remplace TripoSR,
	//    pas une ligne du modeleur ne bouge.
	//
	//    Rodolf a demande EXPLICITEMENT la meme propriete pour le design :
	//    « et ca doit etre pareil pour le design UI ». Ce dorsal-ci ne connait
	//    donc ni Qwen, ni Ollama, ni GGUF, ni HTTP : deux trous, `{invite}` et
	//    `{sortie}`, et la meme regle.
	//
	// ET IL Y A UNE SECONDE RAISON, MESUREE CELLE-LA (14/09) : le moteur local
	//    occupe **4 444 Mo de VRAM sur une carte de 8 Go**. L'en-tete de
	//    `Applications/NKQwen2Chat` porte la mesure de Rodolf du 9 aout : deux
	//    instances sur cette carte, « le pilote Vulkan ACCEPTE quand meme
	//    l'allocation en debordant sur la memoire systeme. Aucun appel n'echoue
	//    [...] mais le calcul lit n'importe quoi, et la generation sort
	//    !!!!!!! ». NkUIDesign est elle-meme une application GPU : charger le
	//    modele DANS son processus, ce serait signer cet echec-qui-se-presente-
	//    en-vert. Le processus externe le charge, repond, et meurt.
	//
	//    DETTE DECLAREE, la meme que le pont 3D : l'attente est SYNCHRONE. Le
	//    chargement du modele mesure 12,6 s, la generation 597 ms par token.
	//    Une conversation (quelques dizaines de tokens) se compte en secondes ;
	//    un document complet se compterait en minutes, et ce fichier ne le cache
	//    pas.
	class NkConverseBackendProcessus final : public NkIConverseBackend {
		public:
			/// Le gabarit, deux trous : `{invite}` (le fichier qu'on ecrit) et
			/// `{sortie}` (le fichier qu'on attend).
			NkString gabarit;
			NkString nom = NkString("processus");
			NkString invitePath = NkString("nkuidesign_invite.txt");
			NkString sortiePath = NkString("nkuidesign_sortie.txt");
			int32 dernierCode = -1; ///< code de sortie du dernier lancement

			bool Complete(const NkConverseRequest &req, NkConverseReply &out) override {
				out.success = false;
				out.text = NkString("");
				if (gabarit.Length() == 0) {
					out.error = NkString("REFUS : aucune commande de generation (NK_DESIGN_CMD vide)");
					return false;
				}
				// L'invite COMPLETE, assemblee par la MEME fonction que le dorsal
				// fichier : deux assemblages auraient diverge au premier champ
				// ajoute a NkConverseRequest, et une reponse rejouee a la main
				// n'aurait plus correspondu a celle du processus.
				NkString full;
				EcrireRequete(req, full);
				if (!nkentseu::NkFile::WriteAllText(invitePath.Data(), full.Data())) {
					out.error = NkString("REFUS : impossible d'ecrire l'invite dans ");
					out.error.Append(invitePath);
					return false;
				}
				// EFFACER LA SORTIE AVANT. Sans ca, un fichier laisse par un
				// lancement precedent ferait passer un echec pour une reussite --
				// et rendrait la MEME reponse indefiniment. C'est la garde du pont
				// 3D, reprise telle quelle.
				if (nkentseu::NkFile::Exists(sortiePath.Data()))
					nkentseu::NkFile::Delete(sortiePath.Data());

				NkString cmd = gabarit;
				Remplir(cmd, "{invite}", invitePath.Data());
				Remplir(cmd, "{sortie}", sortiePath.Data());
				dernierCode = -1;
				if (!Lancer(cmd.Data(), dernierCode)) {
					out.error = NkString("REFUS : le processus n'a pas pu etre lance : ");
					out.error.Append(cmd);
					return false;
				}
				if (!nkentseu::NkFile::Exists(sortiePath.Data())) {
					char b[112];
					snprintf(b, sizeof(b),
							 "REFUS : le generateur a rendu %d et n'a pas ecrit ",
							 (int)dernierCode);
					out.error = NkString(b);
					out.error.Append(sortiePath);
					return false;
				}
				out.text = nkentseu::NkFile::ReadAllText(sortiePath.Data());
				out.success = out.text.Length() > 0;
				if (!out.success)
					out.error = NkString("REFUS : le fichier de sortie est vide");
				return out.success;
			}

			bool IsAvailable() const override {
				return gabarit.Length() > 0;
			}
			const char *Name() const override {
				return nom.Data() ? nom.Data() : "processus";
			}

			/// L'assemblage de la requete, PARTAGE avec le dorsal fichier.
			static void EcrireRequete(const NkConverseRequest &req, NkString &full) {
				full = NkString("");
				full.Append(req.prompt);
				if (req.catalog.Length() > 0) {
					full.Append("\n\n--- composants declares ---\n");
					full.Append(req.catalog);
				}
				if (req.currentDoc.Length() > 0) {
					full.Append("\n--- document courant ---\n");
					full.Append(req.currentDoc);
				}
			}

			/// Remplace CHAQUE occurrence de `trou` par `val`.
			static void Remplir(NkString &s, const char *trou, const char *val) {
				const NkString::SizeType lt = (NkString::SizeType)StrLen(trou);
				NkString::SizeType pos = 0;
				for (;;) {
					NkString::SizeType i = s.Find(trou, pos);
					if (i == NkString::npos)
						break;
					s.Replace(i, lt, val ? val : "");
					pos = i + (NkString::SizeType)StrLen(val);
				}
			}

			/// LE DORSAL PAR DEFAUT, configure depuis l'environnement, une fois.
			/// C'est le SEUL endroit qui sait quel generateur tourne -- le
			/// panneau, lui, n'affiche que `Name()`.
			static NkConverseBackendProcessus &ParDefaut() {
				static NkConverseBackendProcessus g;
				static bool init = false;
				if (!init) {
					init = true;
					if (const char *c = getenv("NK_DESIGN_CMD")) {
						g.gabarit = NkString(c);
					} else if (const char *e = getenv("NK_DESIGN_EXE")) {
						g.gabarit = NkString("\"");
						g.gabarit.Append(e);
						g.gabarit.Append("\" --invite \"{invite}\" --sortie \"{sortie}\"");
					}
					if (const char *n = getenv("NK_DESIGN_NOM"))
						g.nom = NkString(n);
				}
				return g;
			}

		private:
			static nkentseu::usize StrLen(const char *s) {
				nkentseu::usize n = 0;
				while (s && s[n])
					++n;
				return n;
			}
			/// Lance la ligne et ATTEND sa fin. Rend faux si le lancement lui-meme
			/// echoue ; `code` recoit le code de sortie du processus.
			static bool Lancer(const char *ligne, int32 &code) {
#ifdef _WIN32
				const int n = MultiByteToWideChar(CP_UTF8, 0, ligne, -1, nullptr, 0);
				if (n <= 0)
					return false;
				wchar_t *w = new wchar_t[(nkentseu::usize)n];
				MultiByteToWideChar(CP_UTF8, 0, ligne, -1, w, n);
				STARTUPINFOW si;
				PROCESS_INFORMATION pi;
				memset(&si, 0, sizeof(si));
				si.cb = sizeof(si);
				memset(&pi, 0, sizeof(pi));
				// CREATE_NO_WINDOW : pas de console qui surgit devant l'editeur.
				const BOOL ok = CreateProcessW(nullptr, w, nullptr, nullptr, FALSE,
											   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
				delete[] w;
				if (!ok)
					return false;
				WaitForSingleObject(pi.hProcess, INFINITE);
				DWORD ec = (DWORD)-1;
				GetExitCodeProcess(pi.hProcess, &ec);
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
				code = (int32)ec;
				return true;
#else
				const int r = system(ligne);
				if (r == -1)
					return false;
				code = (int32)r;
				return true;
#endif
			}
	};

	// ── BACKEND EN CONSERVE ─────────────────────────────────────────────────
	// ⚠️ CELUI-CI EST UN INSTRUMENT DE MESURE, pas un backend de travail : il rend
	//    la reponse qu'on lui a posee. Il existe pour que la sonde puisse exercer
	//    la chaine complete — prompt, porte, verification, provenance, rejet —
	//    sans reseau et sans modele. **Condition de retrait : aucune.** Le jour ou
	//    un vrai backend existe, celui-ci reste, exactement comme un peintre
	//    enregistreur reste a cote d'un vrai peintre : c'est ce qui permet de
	//    tester le rejet sans avoir a provoquer une mauvaise reponse chez un
	//    modele.
	class NkConverseBackendConserve final : public NkIConverseBackend {
		public:
			NkString canned;
			bool available = true;
			int32 calls = 0;

			bool Complete(const NkConverseRequest &, NkConverseReply &out) override {
				++calls;
				out.text = canned;
				out.success = available && canned.Length() > 0;
				if (!out.success)
					out.error = NkString("backend en conserve : rien a rendre");
				return out.success;
			}
			bool IsAvailable() const override {
				return available;
			}
			const char *Name() const override {
				return "conserve";
			}
	};

	// ── LE VERDICT ──────────────────────────────────────────────────────────
	// Une proposition n'est pas « acceptee ou refusee » : elle est refusee POUR
	// UNE RAISON, et la raison est ce qui permet d'ameliorer le prompt (ou de
	// constater que le modele n'est pas pret). Un booleen aurait rendu tout ca
	// muet.
} // namespace nkentseu::converse
