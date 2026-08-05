#pragma once
// =============================================================================
// NkAiAccounts.h — PLUSIEURS comptes Claude Code, un par usage.
//
// Principe (verifie sur le binaire natif du CLI, pas suppose) :
//   - l'authentification vit dans <config>/.credentials.json ;
//   - CLAUDE_CONFIG_DIR deplace CE dossier.
// Donner un dossier par compte suffit donc a les isoler : chaque instance de
// NKCode lance l'agent avec le CLAUDE_CONFIG_DIR de SON compte.
//
// ── Memoire PARTAGEE entre comptes ───────────────────────────────────────────
// La memoire automatique vit SOUS le dossier de configuration
// (<config>/projects/<workspace>/memory/) : isoler la config isolerait donc
// aussi la memoire, ce qui n'est pas voulu — on veut des identites separees
// mais un SAVOIR commun.
//
// Le CLI expose bien CLAUDE_MEMORY_STORES (tableau JSON de chemins), MAIS son
// code montre que la definir DESACTIVE la memoire automatique par projet et
// bascule en mode « depots d'equipe » :
//     if (process.env.CLAUDE_MEMORY_STORES?.trim()) { ...; return null; }
// Ce n'est pas le comportement recherche.
//
// On partage donc au niveau du SYSTEME DE FICHIERS : le sous-dossier
// `projects` de chaque compte est une JONCTION vers un dossier commun. Le CLI
// ne voit qu'un dossier ordinaire, son comportement ne change pas, et memoire,
// historique et transcripts sont communs a tous les comptes.
//
// Arborescence :
//   ~/.nkcode/accounts/accounts.cfg     registre (un nom par ligne, 1er = defaut)
//   ~/.nkcode/accounts/<nom>/           CLAUDE_CONFIG_DIR du compte
//   ~/.nkcode/accounts/<nom>/projects   -> jonction vers _shared/projects
//   ~/.nkcode/accounts/_shared/projects memoire/historique COMMUNS
//
// Le choix du compte est PAR WORKSPACE (.nkcode/ai_account.txt) : deux
// instances de NKCode ouvertes sur deux projets peuvent viser deux comptes
// differents, en meme temps.
// =============================================================================
#include "NKCode/Shell/NkOpenWs.h" // NkOpenWsState::Home()
#include "NKCode/Shell/NkShell.h"  // NkCodeShellRun (creation de la jonction)
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;

		struct NkAiAccount {
				NkString name;	 // libelle choisi par l'utilisateur (« Perso », « Client X »)
				NkString dir;	 // CLAUDE_CONFIG_DIR de ce compte
				bool connected;	 // .credentials.json present = deja authentifie
		};

		// Racine des comptes : ~/.nkcode/accounts
		inline NkString NkAiAccountsRoot() {
			return (NkPath(NkOpenWsState::Home().CStr()) / ".nkcode" / "accounts").ToString();
		}

		inline NkString NkAiAccountsRegistry() {
			return (NkPath(NkAiAccountsRoot().CStr()) / "accounts.cfg").ToString();
		}

		// Dossier COMMUN a tous les comptes (memoire, historique, transcripts).
		inline NkString NkAiSharedProjects() {
			return (NkPath(NkAiAccountsRoot().CStr()) / "_shared" / "projects").ToString();
		}

		inline NkString NkAiAccountDir(const NkString &name) {
			return (NkPath(NkAiAccountsRoot().CStr()) / name.CStr()).ToString();
		}

		// Un compte est CONNECTE des lors que le CLI y a depose ses identifiants.
		// On ne les lit pas : leur simple presence suffit, et ils ne nous regardent pas.
		inline bool NkAiAccountConnected(const NkString &name) {
			return NkFile::Exists((NkPath(NkAiAccountDir(name).CStr()) / ".credentials.json").ToString().CStr());
		}

		// Nom valide pour un dossier : on refuse tout ce qui pourrait s'echapper de
		// la racine des comptes (« .. », separateurs) plutot que de le nettoyer en
		// silence — un nom refuse se corrige, un nom transforme surprend.
		inline bool NkAiAccountNameValid(const char *n) {
			if (!n || !*n)
				return false;
			for (const char *p = n; *p; ++p) {
				const char c = *p;
				const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
								c == '_' || c == ' ';
				if (!ok)
					return false;
			}
			return true;
		}

		inline NkVector<NkString> NkAiAccountNames() {
			NkVector<NkString> out;
			const NkString f = NkAiAccountsRegistry();
			if (!NkFile::Exists(f.CStr()))
				return out;
			const NkString txt = NkFile::ReadAllText(NkPath(f.CStr()));
			NkString cur;
			for (const char *p = txt.CStr();; ++p) {
				if (*p == '\n' || *p == '\r' || *p == '\0') {
					const NkString t = cur.Trim();
					if (!t.Empty()) {
						bool dup = false;
						for (usize i = 0; i < out.Size(); ++i)
							if (out[i] == t) {
								dup = true;
								break;
							}
						if (!dup)
							out.PushBack(t);
					}
					cur.Clear();
					if (!*p)
						break;
					continue;
				}
				cur += *p;
			}
			return out;
		}

		inline void NkAiAccountsSave(const NkVector<NkString> &names) {
			NkString o;
			for (usize i = 0; i < names.Size(); ++i) {
				o += names[i];
				o += "\n";
			}
			NkDirectory::CreateRecursive(NkAiAccountsRoot().CStr());
			NkFile::WriteAllText(NkPath(NkAiAccountsRegistry().CStr()), o);
		}

		// Fait pointer <compte>/projects vers le dossier COMMUN. Sans effet si la
		// jonction existe deja. Si un VRAI dossier `projects` est present (compte
		// cree avant cette fonctionnalite), on n'y touche pas : ecraser des donnees
		// existantes serait pire que de ne pas partager.
		inline void NkAiEnsureSharedMemory(const NkString &name) {
			const NkString shared = NkAiSharedProjects();
			NkDirectory::CreateRecursive(shared.CStr());
			const NkString link = (NkPath(NkAiAccountDir(name).CStr()) / "projects").ToString();
			if (NkDirectory::Exists(link.CStr()) || NkFile::Exists(link.CStr()))
				return; // deja en place (jonction ou vrai dossier) : on ne detruit rien
#if defined(_WIN32)
			// Jonction (/J) : ne demande AUCUN privilege administrateur, contrairement
			// au lien symbolique.
			NkString cmd = "mklink /J \"";
			cmd += link;
			cmd += "\" \"";
			cmd += shared;
			cmd += "\"";
#else
			NkString cmd = "ln -s \"";
			cmd += shared;
			cmd += "\" \"";
			cmd += link;
			cmd += "\"";
#endif
			NkCodeShellRun(cmd.CStr());
		}

		// Cree le compte (dossier + entree de registre + memoire partagee).
		// Idempotent : rappeler avec un nom existant ne fait que garantir l'etat.
		inline bool NkAiAccountCreate(const NkString &name) {
			if (!NkAiAccountNameValid(name.CStr()))
				return false;
			NkDirectory::CreateRecursive(NkAiAccountDir(name).CStr());
			NkVector<NkString> names = NkAiAccountNames();
			bool present = false;
			for (usize i = 0; i < names.Size(); ++i)
				if (names[i] == name) {
					present = true;
					break;
				}
			if (!present) {
				names.PushBack(name);
				NkAiAccountsSave(names);
			}
			NkAiEnsureSharedMemory(name);
			return true;
		}

		inline NkVector<NkAiAccount> NkAiAccounts() {
			NkVector<NkAiAccount> out;
			const NkVector<NkString> names = NkAiAccountNames();
			for (usize i = 0; i < names.Size(); ++i) {
				NkAiAccount a;
				a.name = names[i];
				a.dir = NkAiAccountDir(names[i]);
				a.connected = NkAiAccountConnected(names[i]);
				out.PushBack(a);
			}
			return out;
		}

		// Compte PAR DEFAUT = le PREMIER CONNECTE (demande de Rihen). Un compte
		// declare mais jamais authentifie ne peut rien faire : le proposer par
		// defaut ne ferait qu'echouer au premier message.
		inline NkString NkAiDefaultAccount() {
			const NkVector<NkString> names = NkAiAccountNames();
			for (usize i = 0; i < names.Size(); ++i)
				if (NkAiAccountConnected(names[i]))
					return names[i];
			return NkString();
		}

		// ── Choix PAR WORKSPACE ─────────────────────────────────────────────────
		// Deux instances de NKCode ouvertes sur deux projets visent deux comptes
		// differents EN MEME TEMPS : le choix appartient donc au workspace, pas a
		// l'application.
		inline NkString NkAiWorkspaceAccountFile(const NkPath &wsRoot) {
			return (wsRoot / ".nkcode" / "ai_account.txt").ToString();
		}

		inline NkString NkAiWorkspaceAccount(const NkPath &wsRoot) {
			const NkString f = NkAiWorkspaceAccountFile(wsRoot);
			if (NkFile::Exists(f.CStr())) {
				const NkString n = NkFile::ReadAllText(NkPath(f.CStr())).Trim();
				if (!n.Empty() && NkAiAccountConnected(n))
					return n; // le compte a pu etre supprime depuis : on verifie
			}
			return NkAiDefaultAccount();
		}

		inline void NkAiSetWorkspaceAccount(const NkPath &wsRoot, const NkString &name) {
			NkDirectory::CreateRecursive((wsRoot / ".nkcode").ToString().CStr());
			NkFile::WriteAllText(NkPath(NkAiWorkspaceAccountFile(wsRoot).CStr()), name);
		}

		// Commande de CONNEXION a lancer dans le terminal dedie : le navigateur
		// s'ouvre, l'utilisateur colle son code, et le CLI ecrit les identifiants
		// dans le dossier du compte. Volontairement VISIBLE plutot que masquee par
		// une barre de progression — c'est un echange interactif.
		inline NkString NkAiLoginCommand(const NkString &exe, const NkString &name) {
			const NkString dir = NkAiAccountDir(name);
#if defined(_WIN32)
			NkString c = "set \"CLAUDE_CONFIG_DIR=";
			c += dir;
			c += "\" && \"";
			c += exe;
			c += "\" /login";
#else
			NkString c = "CLAUDE_CONFIG_DIR=\"";
			c += dir;
			c += "\" \"";
			c += exe;
			c += "\" /login";
#endif
			return c;
		}

	} // namespace nkcode
} // namespace nkentseu
