#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverseClaude.h
// @Brief   LE DORSAL CLAUDE -- le CLI de Claude Code, lance comme un processus.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE FICHIER NE REECRIT RIEN. IL COLLE TROIS PIECES QUI EXISTENT DEJA.
// =============================================================================
//  1. LE LANCEUR : `NkConverseBackendProcessus` (NkConverse.h). Il ecrit
//     l'invite dans `{invite}`, EFFACE `{sortie}` avant (sans quoi un fichier
//     laisse par le lancement precedent ferait passer un echec pour une
//     reussite), lance la ligne, attend, relit. Ce dorsal-ci le COMPOSE ; il
//     n'ecrit pas un second CreateProcess.
//  2. LE MECANISME DE COMPTES : invente et VERIFIE par NKCode
//     (`Applications/NKCode/src/NKCode/Shell/NkAiAccounts.h`, 374 lignes,
//     « verifie sur le binaire natif du CLI, pas suppose »). Son principe tient
//     en deux lignes : l'authentification vit dans `<config>/.credentials.json`,
//     et `CLAUDE_CONFIG_DIR` deplace CE dossier.
//  3. LA RESOLUTION DE L'EXECUTABLE : les pieges Windows du paquet npm, payes
//     et documentes par NKCode (shim .cmd non lancable par CreateProcessW,
//     script-garde de 500 octets qui ouvre une boite modale bloquante).
//
//  ⚠️ POURQUOI ON NE FAIT PAS `#include "NKCode/Shell/NkAiAccounts.h"` : ce
//     fichier vit dans une APPLICATION et inclut `NkOpenWs.h` et `NkShell.h`.
//     Une application n'est pas une bibliotheque ; NK3DModeler ne peut pas
//     dependre de NKCode. On reprend donc le MECANISME, pas le fichier -- et on
//     lit LE MEME DOSSIER SUR LE DISQUE, `~/.nkcode/accounts`, pour que les
//     comptes connectes dans NKCode soient ceux d'ici.
//
//  ⚠️ ET CE N'EST PAS UNE SECONDE LISTE QUI POURRAIT DIVERGER. NkAiAccounts.h
//     ecrit lui-meme sa regle : « La liste est donc DERIVEE du disque ». Deux
//     lecteurs d'une meme verite de disque ne divergent pas ; deux REGISTRES
//     tenus separement, si. Ce fichier ne CREE aucun compte et n'en inscrit
//     aucun : il ne fait que LIRE ceux que NKCode a crees. La creation, la
//     jonction de memoire partagee et la commande de connexion restent chez
//     leur proprietaire.
//
// =============================================================================
//  ⚠️⚠️ CE QUE CE FICHIER NE FAIT JAMAIS, ET C'EST LA CONDITION DU LOT
// =============================================================================
//  IL NE TOUCHE AUCUNE CLE, AUCUN IDENTIFIANT, AUCUN JETON.
//    - Il ne LIT jamais `.credentials.json` : il teste seulement s'il EXISTE.
//      Sa presence dit « ce compte est authentifie », et c'est tout ce dont on
//      a besoin. Son contenu ne nous regarde pas.
//    - Il ne passe JAMAIS de secret en argument de ligne de commande. Une cle
//      en argument serait visible dans la table des processus de la machine --
//      c'est la raison pour laquelle cette route a ete ecartee explicitement.
//    - Il ne pose qu'une seule variable d'environnement, `CLAUDE_CONFIG_DIR`,
//      et sa valeur est un CHEMIN DE DOSSIER. C'est le CLI qui detient les
//      identifiants de Rodolf ; nous ne faisons que le lancer au bon endroit.
//  Si une evolution de ce fichier amene a lire, stocker, copier ou passer une
//  cle, c'est que le chemin est faux : il faut s'arreter et le dire.
//
// =============================================================================
//  ⚠️ CLAUDE EST UN SERVICE DISTANT, ET L'INTERFACE DOIT LE DIRE
// =============================================================================
//  Le dorsal local annonce « rien ne quitte cette machine ». Celui-ci fait
//  l'inverse, et l'utilisateur doit pouvoir le choisir EN LE SACHANT, a chaque
//  usage -- pas l'apprendre apres. `NkClaudeAvertissement()` porte la phrase ;
//  `DerniereInvite().Length()` porte le CHIFFRE de ce qui est reellement parti.
//  Un avertissement general se survole ; « 2 431 octets sont partis » se lit.
//
// =============================================================================
//  ⚠️ LES DRAPEAUX DU CLI, ET POURQUOI CHACUN EST LA
// =============================================================================
//  `claude -p` lance par defaut un AGENT : il decouvre le `CLAUDE.md` du
//  repertoire courant, charge competences, greffons, crochets et serveurs MCP,
//  et dispose d'outils qui peuvent ECRIRE SUR LE DISQUE. Rien de tout cela
//  n'est voulu ici : on veut une COMPLETION -- une invite, une ligne.
//    --safe-mode                ni CLAUDE.md, ni competence, ni greffon, ni
//                               crochet, ni MCP. Sans lui, l'outil repondrait
//                               en fonction du projet ouvert a cote.
//    --tools ""                 AUCUN outil. C'est la garde qui empeche une
//                               reponse de devenir une action sur le disque.
//    --strict-mcp-config        aucun serveur MCP, meme configure ailleurs.
//    --no-session-persistence   ne laisse pas de session derriere lui.
//    --output-format text       la reponse brute, pas du JSON a reanalyser.
//  ⚠️ `--model` EST UN REGLAGE, JAMAIS UNE CONSTANTE -- meme regle que
//     `NkConverseBackendOllama` : un nom grave ici ferait un dorsal-a-un-modele
//     qu'il faudrait recompiler pour en essayer un autre.
//
//  ⚠️ POURQUOI UN SCRIPT ET PAS UNE LIGNE DE COMMANDE. `NkConverseBackend-
//     Processus::Lancer` appelle `CreateProcessW` SANS bloc d'environnement :
//     il n'y a aucun endroit ou poser `CLAUDE_CONFIG_DIR`. Et `set` est une
//     commande interne de cmd, pas un executable -- une ligne
//     `set "X=Y" && exe` ne lancerait donc RIEN. NKCode a paye exactement ce
//     defaut (« [processus termine] » instantane) et sa reponse etait un petit
//     script : on la reprend. Avantage supplementaire, plus aucune imbrication
//     de guillemets a faire survivre a cmd.exe.
//     ⚠️ La forme `cmd.exe /C ""script" "invite" "sortie""` est VERIFIEE a
//        travers CreateProcessW, avec des espaces dans les trois chemins --
//        pas supposee d'apres la documentation de cmd.
// -----------------------------------------------------------------------------

#include "NKConverse/NkConverse.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"

#include <cstdio>
#include <cstdlib>

namespace nkentseu::converse {

	// ═══════════════════════════════════════════════════════════════════════
	//  L'EXECUTABLE DU CLI
	// ═══════════════════════════════════════════════════════════════════════
	//  ⚠️ UN FICHIER .exe QUI EXISTE N'EST PAS FORCEMENT UN EXECUTABLE. Le
	//     paquet npm livre a `bin\claude.exe` un SCRIPT-GARDE de ~500 octets
	//     quand le postinstall n'a pas tourne. Le donner a CreateProcessW ouvre
	//     une boite MODALE Windows « Application 16 bits non prise en charge »,
	//     bloquante. On exige donc la signature PE « MZ ». Piege releve et paye
	//     par NKCode ; on ne le repaye pas.
	inline bool NkClaudeExeNatif(const NkString &chemin) {
		if (chemin.Length() == 0 || !nkentseu::NkFile::Exists(chemin.Data()))
			return false;
		std::FILE *f = std::fopen(chemin.Data(), "rb");
		if (!f)
			return false;
		char sig[2] = {0, 0};
		const nkentseu::usize n = std::fread(sig, 1, 2, f);
		std::fclose(f);
		return n == 2 && sig[0] == 'M' && sig[1] == 'Z';
	}

	/// Le chemin NATIF du CLI, resolu UNE FOIS. Chaine vide = « CLI absent »,
	/// qui est un refus nomme et non une panne : l'appelant le dit et propose
	/// le geste qui repare.
	inline const NkString &NkClaudeExe() {
		static NkString sChemin;
		static bool sResolu = false;
		if (sResolu)
			return sChemin;
		sResolu = true;
		// Un REGLAGE d'abord : une installation hors des chemins npm usuels se
		// declare, elle ne se devine pas.
		if (const char *e = std::getenv("NK_CLAUDE_EXE"))
			if (*e && NkClaudeExeNatif(NkString(e))) {
				sChemin = NkString(e);
				return sChemin;
			}
#if defined(_WIN32)
		// ⚠️ Le shim npm « claude.cmd » n'est PAS lancable par CreateProcessW :
		//    CreateProcess n'essaie que l'extension .exe implicite, jamais .cmd
		//    ni PATHEXT. On vise le VRAI binaire natif. Deux emplacements : le
		//    second (la dependance optionnelle par plateforme) est le plus
		//    fiable -- le premier n'est renseigne que si le postinstall a
		//    reussi sa copie finale.
		static const char *const kRel[] = {
			"\\npm\\node_modules\\@anthropic-ai\\claude-code\\bin\\claude.exe",
			"\\npm\\node_modules\\@anthropic-ai\\claude-code\\node_modules\\@anthropic-ai"
			"\\claude-code-win32-x64\\claude.exe",
			"\\npm\\node_modules\\@anthropic-ai\\claude-code\\node_modules\\@anthropic-ai"
			"\\claude-code-win32-arm64\\claude.exe",
		};
		if (const char *appData = std::getenv("APPDATA"))
			if (*appData)
				for (nkentseu::usize i = 0; i < sizeof(kRel) / sizeof(kRel[0]); ++i) {
					NkString c(appData);
					c.Append(kRel[i]);
					if (NkClaudeExeNatif(c)) {
						sChemin = c;
						return sChemin;
					}
				}
		// Aucun binaire natif VALIDE : on rend une chaine VIDE. Rendre « claude »
		// ferait retomber CreateProcessW sur le script-garde, c'est-a-dire sur la
		// boite modale bloquante. Un refus nomme vaut mieux qu'un lancement qui
		// bloque la fenetre de Rodolf.
		return sChemin;
#else
		// Unix : pas de piege d'extension, le shim est executable.
		sChemin = NkString("claude");
		return sChemin;
#endif
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  LES COMPTES -- ON LES LIT, ON N'EN CREE AUCUN
	// ═══════════════════════════════════════════════════════════════════════
	//  Le MEME dossier que NKCode : `~/.nkcode/accounts/<nom>/`. Un compte
	//  connecte dans NKCode est utilisable ici sans rien refaire.

	inline NkString NkClaudeComptesRacine() {
		return (nkentseu::NkDirectory::GetHomeDirectory() / ".nkcode" / "accounts").ToString();
	}

	inline NkString NkClaudeCompteDir(const NkString &nom) {
		if (nom.Length() == 0)
			return NkString(); // vide = le dossier par defaut du CLI (~/.claude)
		return (nkentseu::NkPath(NkClaudeComptesRacine().Data()) / nom.Data()).ToString();
	}

	/// ⚠️ ON NE LIT PAS LE FICHIER : on teste s'il EXISTE. Sa presence suffit a
	///    dire « ce compte est authentifie », et son contenu ne nous regarde pas.
	inline bool NkClaudeCompteConnecte(const NkString &nom) {
		const NkString d = NkClaudeCompteDir(nom);
		nkentseu::NkPath base = d.Length() == 0
									? (nkentseu::NkDirectory::GetHomeDirectory() / ".claude")
									: nkentseu::NkPath(d.Data());
		return nkentseu::NkFile::Exists((base / ".credentials.json").ToString().Data());
	}

	/// Les comptes CONNECTES, derives du disque. Le compte nomme "" (le dossier
	/// par defaut du CLI) n'y figure pas : il est teste a part, parce qu'il
	/// existe sans que NKCode ait jamais tourne.
	inline nkentseu::NkVector<NkString> NkClaudeComptes() {
		nkentseu::NkVector<NkString> out;
		const NkString racine = NkClaudeComptesRacine();
		if (!nkentseu::NkDirectory::Exists(racine.Data()))
			return out;
		const nkentseu::NkVector<nkentseu::NkDirectoryEntry> e =
			nkentseu::NkDirectory::GetEntries(racine.Data());
		for (nkentseu::usize i = 0; i < e.Size(); ++i) {
			if (!e[i].IsDirectory || e[i].Name == "_shared" || e[i].Name == "." || e[i].Name == "..")
				continue;
			if (NkClaudeCompteConnecte(e[i].Name))
				out.PushBack(e[i].Name);
		}
		return out;
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  LE ZERO, ET IL DIT LEQUEL
	// ═══════════════════════════════════════════════════════════════════════
	//  ⚠️ TROIS ETATS QUI SE REPARENT A TROIS ENDROITS DIFFERENTS. Les
	//     confondre sous « Claude indisponible » enverrait chercher un defaut
	//     de reseau quand il manque une connexion, et inversement.
	//  ⚠️ ET LE CONSEIL PASSE DEVANT LE DETAIL. Ces motifs s'affichent dans une
	//     ligne d'etat qui TRONQUE. Un motif qui commence par le diagnostic
	//     perd, a la coupure, la seule partie utile : le geste qui repare.
	//     Chaque phrase ci-dessous commence donc par l'imperatif.
	enum class NkClaudeEtat : nkentseu::uint8 {
		Pret = 0,	   ///< binaire natif present ET un compte authentifie
		CliAbsent,	   ///< le paquet npm n'est pas installe (ou pas jusqu'au bout)
		NonConnecte	   ///< le CLI est la, personne ne s'est authentifie
	};

	inline NkClaudeEtat NkClaudeDiagnostic(const NkString &compte, NkString &quoiFaire) {
		if (NkClaudeExe().Length() == 0) {
			quoiFaire = NkString("Installez Claude Code, puis relancez : "
								 "npm install -g @anthropic-ai/claude-code "
								 "(le CLI n'est pas sur cette machine, ou son binaire natif manque)");
			return NkClaudeEtat::CliAbsent;
		}
		if (!NkClaudeCompteConnecte(compte)) {
			quoiFaire = NkString("Connectez-vous : lancez « claude auth login » dans un terminal "
								 "(ou ouvrez Comptes dans NKCode). Aucun compte authentifie");
			if (compte.Length() > 0) {
				quoiFaire.Append(" pour « ");
				quoiFaire.Append(compte);
				quoiFaire.Append(" »");
			}
			return NkClaudeEtat::NonConnecte;
		}
		quoiFaire = NkString("");
		return NkClaudeEtat::Pret;
	}

	/// LA PHRASE QUE LE PANNEAU DOIT AFFICHER. Elle vit ici et non dans chaque
	/// application : trois formulations differentes du meme avertissement
	/// finiraient par en avoir une qui ment.
	inline const char *NkClaudeAvertissement() {
		return "SERVICE DISTANT : l'invite QUITTE cette machine et part chez Anthropic.";
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  LE DORSAL
	// ═══════════════════════════════════════════════════════════════════════
	//  Meme forme que `NkConverseBackendOllama`, volontairement : un second
	//  patron pour la meme chose serait une divergence gratuite.
	class NkConverseBackendClaude final : public NkIConverseBackend {
		public:
			/// Le compte. Vide = le dossier de configuration par defaut du CLI.
			/// UN REGLAGE, par application ou par espace de travail.
			NkString compte;
			/// Le modele. UN REGLAGE, jamais une constante (voir l'en-tete).
			NkString modele = NkString("sonnet");
			NkString nom = NkString("claude");
			/// L'EFFORT DU PANNEAU (21/09, Q5) : `--effort <niveau>` du CLI, dont
			/// l'aide documente « low, medium, high, xhigh, max ». Vide = non ecrit,
			/// le CLI garde son defaut. Il est ECRIT dans le script : c'est ce que
			/// la preuve relit.
			NkString effort;
			/// Les trois fichiers de travail. L'appelant les pose dans `logs/` :
			/// a la racine ils saliraient le dossier de projet de Rodolf et
			/// reapparaitraient apres chaque nettoyage.
			NkString invitePath = NkString("logs/nkclaude_invite.txt");
			NkString sortiePath = NkString("logs/nkclaude_reponse.txt");
			NkString scriptPath = NkString("logs/nkclaude_lance.cmd");
			/// Publies pour que l'appelant puisse les IMPRIMER : un temps de
			/// reponse sans sa condition ne vaut rien.
			nkentseu::int32 dernierCode = -1;

			bool Complete(const NkConverseRequest &req, NkConverseReply &out) override {
				out.success = false;
				out.text = NkString("");
				out.error = NkString("");
				mDerniereInvite = NkString("");
				dernierCode = -1;

				// (1) et (2) LE CLI, ET LE COMPTE. Avant d'ecrire quoi que ce soit :
				//     ecrire l'invite puis echouer laisserait un fichier qui ferait
				//     croire qu'un appel a eu lieu.
				NkString quoiFaire;
				const NkClaudeEtat etat = NkClaudeDiagnostic(compte, quoiFaire);
				if (etat != NkClaudeEtat::Pret) {
					out.error = NkString("REFUS : ");
					out.error.Append(quoiFaire);
					return false;
				}

				if (!EcrireScript()) {
					out.error = NkString("REFUS : impossible d'ecrire le lanceur dans ");
					out.error.Append(scriptPath);
					return false;
				}

				// LE LANCEUR EXISTANT FAIT LE RESTE : il ecrit l'invite par la MEME
				// fonction d'assemblage que les autres dorsaux, efface la sortie
				// AVANT, lance, attend, relit. On ne reecrit pas ces gardes.
				mProc.nom = nom;
				mProc.invitePath = invitePath;
				mProc.sortiePath = sortiePath;
				mProc.gabarit = Gabarit();
				const bool ok = mProc.Complete(req, out);
				dernierCode = mProc.dernierCode;
				// ⚠️ LES OCTETS REELLEMENT ENVOYES, repris du lanceur et non
				//    reconstruits : deux assemblages de la meme regle avaient deja
				//    diverge de 639 octets le 20/09, et le fichier ne prouvait plus
				//    rien.
				mDerniereInvite = mProc.DerniereInvite();
				if (!ok && out.error.Length() == 0)
					out.error = NkString("REFUS : le CLI n'a rien rendu");
				return ok;
			}

			/// ⚠️ « DISPONIBLE » VEUT DIRE PRET A REPONDRE, pas « le fichier
			///    existe ». Un dorsal qui se declare disponible sans compte
			///    connecte serait choisi, puis echouerait au premier message --
			///    et l'utilisateur chercherait le defaut dans son invite.
			bool IsAvailable() const override {
				NkString ignore;
				return NkClaudeDiagnostic(compte, ignore) == NkClaudeEtat::Pret;
			}

			const char *Name() const override {
				return nom.Data() ? nom.Data() : "claude";
			}

		private:
			NkConverseBackendProcessus mProc; ///< LE LANCEUR, deja ecrit et deja garde

			NkString Gabarit() const {
				NkString g;
#if defined(_WIN32)
				// Forme VERIFIEE a travers CreateProcessW, espaces compris.
				g = NkString("cmd.exe /C \"\"");
				g.Append(scriptPath);
				g.Append("\" \"{invite}\" \"{sortie}\"\"");
#else
				g = NkString("/bin/sh \"");
				g.Append(scriptPath);
				g.Append("\" \"{invite}\" \"{sortie}\"");
#endif
				return g;
			}

			/// Le script est REECRIT a chaque appel. C'est une ecriture de
			/// quelques centaines d'octets, et elle garantit que le lanceur
			/// correspond aux reglages COURANTS -- un script rescape d'un
			/// reglage precedent viserait le mauvais compte en silence.
			bool EcrireScript() const {
				const NkString dir = NkClaudeCompteDir(compte);
				NkString s;
#if defined(_WIN32)
				s = NkString("@echo off\r\n");
				if (dir.Length() > 0) {
					s.Append("set \"CLAUDE_CONFIG_DIR=");
					s.Append(dir);
					s.Append("\"\r\n");
				}
				s.Append("\"");
				s.Append(NkClaudeExe());
				s.Append("\" -p --model \"");
				s.Append(modele);
				if (effort.Length() > 0) {
					s.Append("\" --effort \"");
					s.Append(effort);
				}
				s.Append("\" --safe-mode --tools \"\" --strict-mcp-config"
						 " --no-session-persistence --output-format text"
						 " < \"%~1\" > \"%~2\"\r\n");
#else
				s = NkString("#!/bin/sh\n");
				if (dir.Length() > 0) {
					s.Append("CLAUDE_CONFIG_DIR=\"");
					s.Append(dir);
					s.Append("\"\nexport CLAUDE_CONFIG_DIR\n");
				}
				s.Append("\"");
				s.Append(NkClaudeExe());
				s.Append("\" -p --model \"");
				s.Append(modele);
				if (effort.Length() > 0) {
					s.Append("\" --effort \"");
					s.Append(effort);
				}
				s.Append("\" --safe-mode --tools \"\" --strict-mcp-config"
						 " --no-session-persistence --output-format text"
						 " < \"$1\" > \"$2\"\n");
#endif
				return nkentseu::NkFile::WriteAllText(nkentseu::NkPath(scriptPath.Data()), s);
			}
	};

} // namespace nkentseu::converse
