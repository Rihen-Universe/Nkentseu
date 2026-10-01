// =============================================================================
// NkTerminalShells.cpp — la decouverte des shells, par plateforme. Les
// parseurs (WSL, /etc/shells) sont communs et purs ; windows.h reste confine
// a la partie Windows de ce fichier.
// =============================================================================
#include "NKEditorKit/Terminal/NkTerminalShells.h"
#include "NKEditorKit/Terminal/NkTerminalPty.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(NKENTSEU_TERMINAL_CONPTY)
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <windows.h>
#elif defined(NKENTSEU_TERMINAL_POSIX)
#	include <unistd.h>
#endif

namespace nkentseu {
	namespace editorkit {

		// =====================================================================
		//  COMMUN
		// =====================================================================
		const char *NkShellGenreNom(NkShellGenre g) {
			switch (g) {
				case NkShellGenre::PowerShell7: return "PowerShell 7";
				case NkShellGenre::WindowsPowerShell: return "Windows PowerShell";
				case NkShellGenre::Cmd: return "Invite de commandes";
				case NkShellGenre::GitBash: return "Git Bash";
				case NkShellGenre::Msys2: return "MSYS2";
				case NkShellGenre::Wsl: return "WSL";
				case NkShellGenre::Bash: return "bash";
				case NkShellGenre::Zsh: return "zsh";
				case NkShellGenre::Fish: return "fish";
				case NkShellGenre::Sh: return "sh";
				default: return "shell";
			}
		}

		const char *NkShellGenreSigle(NkShellGenre g) {
			switch (g) {
				case NkShellGenre::PowerShell7:
				case NkShellGenre::WindowsPowerShell: return "PS";
				case NkShellGenre::Cmd: return "C:";
				case NkShellGenre::GitBash: return "G$";
				case NkShellGenre::Msys2: return "M$";
				case NkShellGenre::Wsl: return "W";
				case NkShellGenre::Zsh: return "%";
				case NkShellGenre::Fish: return "><";
				default: return "$";
			}
		}

		NkShellGenre NkTerminalGenreDuChemin(const char *chemin) {
			if (!chemin)
				return NkShellGenre::Autre;
			const char *base = chemin;
			for (const char *p = chemin; *p; ++p)
				if (*p == '/' || *p == '\\')
					base = p + 1;
			char nom[32];
			usize n = 0;
			for (const char *p = base; *p && *p != '.' && n + 1 < sizeof(nom); ++p)
				nom[n++] = (*p >= 'A' && *p <= 'Z') ? static_cast<char>(*p + 32) : *p;
			nom[n] = 0;
			if (std::strcmp(nom, "pwsh") == 0)
				return NkShellGenre::PowerShell7;
			if (std::strcmp(nom, "powershell") == 0)
				return NkShellGenre::WindowsPowerShell;
			if (std::strcmp(nom, "cmd") == 0)
				return NkShellGenre::Cmd;
			if (std::strcmp(nom, "wsl") == 0)
				return NkShellGenre::Wsl;
			if (std::strcmp(nom, "bash") == 0)
				return NkShellGenre::Bash;
			if (std::strcmp(nom, "zsh") == 0)
				return NkShellGenre::Zsh;
			if (std::strcmp(nom, "fish") == 0)
				return NkShellGenre::Fish;
			if (std::strcmp(nom, "sh") == 0 || std::strcmp(nom, "dash") == 0 || std::strcmp(nom, "ash") == 0 ||
				std::strcmp(nom, "ksh") == 0 || std::strcmp(nom, "mksh") == 0 || std::strcmp(nom, "tcsh") == 0 ||
				std::strcmp(nom, "csh") == 0)
				return NkShellGenre::Sh;
			return NkShellGenre::Autre;
		}

		namespace {
			bool NomDeDistroValide(const NkString &s) {
				if (s.Empty() || s.Size() > 64)
					return false;
				for (usize i = 0; i < s.Size(); ++i) {
					const char c = s.CStr()[i];
					const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
									c == '.' || c == '-' || c == '_';
					if (!ok)
						return false;
				}
				return true;
			}

			/// Le nom de base d'un chemin (« /usr/bin/zsh » -> « zsh »).
			NkString Base(const NkString &chemin) {
				const char *b = chemin.CStr();
				for (const char *p = chemin.CStr(); *p; ++p)
					if (*p == '/' || *p == '\\')
						b = p + 1;
				return NkString(b);
			}
		} // namespace

		void NkTerminalLireListeWsl(const char *octets, usize n, NkVector<NkString> &distros) {
			distros.Clear();
			if (!octets || n == 0)
				return;
			const unsigned char *u = reinterpret_cast<const unsigned char *>(octets);
			// UTF-16LE ? BOM FF FE, ou un octet nul sur deux (le cas sans BOM).
			usize nuls = 0;
			for (usize i = 1; i < n; i += 2)
				if (u[i] == 0)
					++nuls;
			const bool utf16 = (n >= 2 && u[0] == 0xFF && u[1] == 0xFE) || (n >= 4 && nuls * 2 >= n / 2);
			NkString ligne;
			auto finir = [&]() {
				// Espaces de fin retires ; un nom invalide (message) est ECARTE.
				usize fin = ligne.Size();
				while (fin > 0 && (ligne.CStr()[fin - 1] == ' ' || ligne.CStr()[fin - 1] == '\t'))
					--fin;
				const NkString nom = ligne.SubStr(0, fin);
				if (NomDeDistroValide(nom))
					distros.PushBack(nom);
				ligne = NkString();
			};
			if (utf16) {
				usize i = (u[0] == 0xFF && u[1] == 0xFE) ? 2 : 0;
				for (; i + 1 < n; i += 2) {
					const uint32 c = static_cast<uint32>(u[i]) | (static_cast<uint32>(u[i + 1]) << 8);
					if (c == '\r' || c == 0xFEFF)
						continue;
					if (c == '\n') {
						finir();
						continue;
					}
					// Un nom valide est ASCII : tout autre caractere invalide la ligne.
					ligne += (c < 0x80) ? static_cast<char>(c) : '?';
				}
			} else {
				usize i = (n >= 3 && u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) ? 3 : 0;
				for (; i < n; ++i) {
					if (u[i] == '\r' || u[i] == 0)
						continue;
					if (u[i] == '\n') {
						finir();
						continue;
					}
					ligne += static_cast<char>(u[i]);
				}
			}
			finir();
		}

		void NkTerminalLireEtcShells(const char *texte, NkVector<NkString> &chemins) {
			chemins.Clear();
			if (!texte)
				return;
			static const char *kEcartes[] = {"nologin", "false", "git-shell", "rbash", "screen", "tmux", "sync", "true"};
			NkVector<NkString> noms;
			const char *s = texte;
			while (*s) {
				const char *e = s;
				while (*e && *e != '\n' && *e != '\r')
					++e;
				const char *a = s;
				while (a < e && (*a == ' ' || *a == '\t'))
					++a;
				const char *b = e;
				while (b > a && (b[-1] == ' ' || b[-1] == '\t'))
					--b;
				if (b > a && *a == '/') {
					NkString chemin;
					for (const char *p = a; p < b; ++p)
						chemin += *p;
					const NkString nom = Base(chemin);
					bool garder = true;
					for (const char *x : kEcartes)
						if (nom == NkString(x))
							garder = false;
					for (usize k = 0; k < noms.Size() && garder; ++k)
						if (noms[k] == nom)
							garder = false;
					if (garder) {
						noms.PushBack(nom);
						chemins.PushBack(chemin);
					}
				}
				while (*e == '\n' || *e == '\r')
					++e;
				s = e;
			}
		}

		// L'invite colorée, par INDICES de palette (34 bleu, 32 vert) : elle suit
		// le theme, sombre comme clair. Elle annonce aussi le dossier courant
		// (OSC 9;9, la convention de Windows Terminal) : l'en-tete l'affiche.
		//
		// Mesure (banc d9, 60 colonnes) : PSReadLine de Windows PowerShell 5.1
		// ne compte PAS l'annonce comme du texte visible -- la commande tapee
		// suit l'invite sans l'ecraser.
		const char *NkTerminalInvitePowerShell() {
			return " -NoLogo -NoExit -Command \"function prompt { $e=[char]27; $p=$PWD.ProviderPath; "
				   "\\\"$e]9;9;$p$e\\$e[34m$p$e[0m$e[32m>$e[0m \\\" }\"";
		}

		const char *NkTerminalInviteCmdTexte() {
			return "$E]9;9;$P$E\\$E[34m$P$E[0m$E[32m$G$E[0m$S";
		}

#if defined(NKENTSEU_TERMINAL_CONPTY)
		// =====================================================================
		//  WINDOWS
		// =====================================================================
		namespace {
			NkString Utf8(const wchar_t *w) {
				NkString out;
				if (!w)
					return out;
				const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
				if (n <= 1)
					return out;
				NkVector<char> b;
				b.Resize(static_cast<usize>(n));
				WideCharToMultiByte(CP_UTF8, 0, w, -1, b.Data(), n, NULL, NULL);
				return NkString(b.Data());
			}

			void Large(const NkString &s, NkVector<wchar_t> &out) {
				out.Clear();
				const int n = MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, NULL, 0);
				out.Resize(static_cast<usize>(n > 0 ? n : 1));
				out[0] = 0;
				if (n > 0)
					MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, out.Data(), n);
			}

			/// Variable d'environnement en UTF-8 (getenv rend la page de code ANSI :
			/// un dossier « Séderis » y serait deja abime).
			NkString Env(const wchar_t *nom) {
				wchar_t b[1024];
				const DWORD n = GetEnvironmentVariableW(nom, b, 1024);
				if (n == 0 || n >= 1024)
					return NkString();
				return Utf8(b);
			}

			bool Existe(const NkString &chemin) {
				if (chemin.Empty())
					return false;
				NkVector<wchar_t> w;
				Large(chemin, w);
				const DWORD a = GetFileAttributesW(w.Data());
				return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
			}

			/// Un programme du PATH (« pwsh.exe »), chemin complet ou vide.
			NkString DansLePath(const wchar_t *exe) {
				wchar_t b[MAX_PATH];
				const DWORD n = SearchPathW(NULL, exe, NULL, MAX_PATH, b, NULL);
				return (n > 0 && n < MAX_PATH) ? Utf8(b) : NkString();
			}

			NkString Guillemets(const NkString &chemin) {
				return NkString("\"") + chemin + "\"";
			}

			const NkString kInvitePs(NkTerminalInvitePowerShell());
			const NkString kInviteCmd = NkString(" /K prompt ") + NkTerminalInviteCmdTexte();

			/// Lance `cmd` SANS fenetre, rend sa sortie et son code. Borne a
			/// `delaiMs` : au-dela, le processus est tue et l'appel rend faux.
			bool LancerSilencieux(const NkString &cmd, NkVector<char> &sortie, DWORD delaiMs, DWORD &code) {
				sortie.Clear();
				code = 1;
				SECURITY_ATTRIBUTES sa;
				sa.nLength = sizeof(sa);
				sa.lpSecurityDescriptor = NULL;
				sa.bInheritHandle = TRUE;
				HANDLE rd = NULL, wr = NULL;
				if (!CreatePipe(&rd, &wr, &sa, 0))
					return false;
				SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
				STARTUPINFOW si;
				ZeroMemory(&si, sizeof(si));
				si.cb = sizeof(si);
				si.dwFlags = STARTF_USESTDHANDLES;
				si.hStdOutput = wr;
				si.hStdError = wr;
				si.hStdInput = NULL;
				PROCESS_INFORMATION pi;
				ZeroMemory(&pi, sizeof(pi));
				NkVector<wchar_t> w;
				Large(cmd, w);
				const BOOL ok = CreateProcessW(NULL, w.Data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
				CloseHandle(wr);
				if (!ok) {
					CloseHandle(rd);
					return false;
				}
				const bool fini = WaitForSingleObject(pi.hProcess, delaiMs) == WAIT_OBJECT_0;
				if (!fini)
					TerminateProcess(pi.hProcess, 1);
				char b[1024];
				DWORD lu = 0;
				while (ReadFile(rd, b, sizeof(b), &lu, NULL) && lu > 0)
					for (DWORD i = 0; i < lu; ++i)
						sortie.PushBack(b[i]);
				GetExitCodeProcess(pi.hProcess, &code);
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
				CloseHandle(rd);
				return fini;
			}
		} // namespace

		NkShellDecouvert NkTerminalShellDeRepli() {
			NkShellDecouvert s;
			s.genre = NkShellGenre::Cmd;
			s.nom = "Invite de commandes";
			NkString cmd = Env(L"ComSpec");
			if (cmd.Empty())
				cmd = "cmd.exe";
			s.chemin = cmd;
			s.commande = Guillemets(cmd) + kInviteCmd;
			s.session = true;
			return s;
		}

		void NkTerminalDecouvrirShells(NkVector<NkShellDecouvert> &out, bool avecWsl) {
			out.Clear();
			const NkString sys = Env(L"SystemRoot");
			const NkString pf = Env(L"ProgramFiles");
			const NkString pf86 = Env(L"ProgramFiles(x86)");
			const NkString lad = Env(L"LOCALAPPDATA");

			// 1. PowerShell 7 : le PATH d'abord (installation par winget, Store),
			//    puis les dossiers d'installation connus.
			{
				NkString pwsh = DansLePath(L"pwsh.exe");
				const NkString cands[] = {pf + "\\PowerShell\\7\\pwsh.exe", pf + "\\PowerShell\\7-preview\\pwsh.exe"};
				for (const NkString &c : cands)
					if (pwsh.Empty() && Existe(c))
						pwsh = c;
				if (!pwsh.Empty()) {
					NkShellDecouvert s;
					s.genre = NkShellGenre::PowerShell7;
					s.nom = "PowerShell 7";
					s.chemin = pwsh;
					s.commande = Guillemets(pwsh) + kInvitePs;
					out.PushBack(s);
				}
			}
			// 2. Windows PowerShell : toujours la (Windows 7 et plus).
			{
				NkString ps = sys + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
				if (!Existe(ps))
					ps = DansLePath(L"powershell.exe");
				if (!ps.Empty()) {
					NkShellDecouvert s;
					s.genre = NkShellGenre::WindowsPowerShell;
					s.nom = "Windows PowerShell";
					s.chemin = ps;
					s.commande = Guillemets(ps) + kInvitePs;
					out.PushBack(s);
				}
			}
			// 3. L'invite de commandes.
			{
				NkShellDecouvert s = NkTerminalShellDeRepli();
				s.session = false;
				out.PushBack(s);
			}
			// 4. Git Bash : trois emplacements d'installation.
			{
				const NkString cands[] = {pf + "\\Git\\bin\\bash.exe", pf86 + "\\Git\\bin\\bash.exe",
										  lad + "\\Programs\\Git\\bin\\bash.exe"};
				for (const NkString &c : cands)
					if (Existe(c)) {
						NkShellDecouvert s;
						s.genre = NkShellGenre::GitBash;
						s.nom = "Git Bash";
						s.chemin = c;
						s.commande = Guillemets(c) + " --login -i";
						out.PushBack(s);
						break;
					}
			}
			// 5. MSYS2 (UCRT64, l'environnement que MSYS2 recommande). `env.exe`
			//    pose MSYSTEM ; CHERE_INVOKING=1 empeche le profil de login de
			//    repartir dans ~ : le shell reste dans le dossier du projet.
			{
				const NkString racines[] = {"C:\\msys64", "C:\\tools\\msys64", Env(L"MSYS2_ROOT")};
				for (const NkString &r : racines)
					if (!r.Empty() && Existe(r + "\\usr\\bin\\bash.exe") && Existe(r + "\\usr\\bin\\env.exe")) {
						NkShellDecouvert s;
						s.genre = NkShellGenre::Msys2;
						s.nom = "MSYS2 (UCRT64)";
						s.chemin = r + "\\usr\\bin\\bash.exe";
						s.commande = Guillemets(r + "\\usr\\bin\\env.exe") +
									 " MSYSTEM=UCRT64 CHERE_INVOKING=1 /usr/bin/bash --login -i";
						out.PushBack(s);
						break;
					}
			}
			// 6. Chaque distribution WSL. La distribution par defaut sort la
			//    premiere de `wsl -l -q`.
			if (avecWsl) {
				const NkString wsl = sys + "\\System32\\wsl.exe";
				NkVector<NkString> distros;
				NkTerminalListerWsl(distros);
				for (usize i = 0; i < distros.Size(); ++i) {
					NkShellDecouvert s;
					s.genre = NkShellGenre::Wsl;
					s.nom = NkString("WSL : ") + distros[i];
					s.distro = distros[i];
					s.chemin = wsl;
					s.commande = Guillemets(wsl) + " -d " + distros[i];
					out.PushBack(s);
				}
			}
			if (!out.Empty())
				out[0].session = true;
		}

		void NkTerminalListerWsl(NkVector<NkString> &distros) {
			distros.Clear();
			const NkString wsl = Env(L"SystemRoot") + "\\System32\\wsl.exe";
			if (!Existe(wsl))
				return;
			NkVector<char> sortie;
			DWORD code = 1;
			// Code de sortie EXIGE : sans WSL, la commande ecrit un message et
			// rend une erreur -- ce message n'est pas une liste.
			if (LancerSilencieux(Guillemets(wsl) + " -l -q", sortie, 4000, code) && code == 0)
				NkTerminalLireListeWsl(sortie.Data(), sortie.Size(), distros);
		}

#elif defined(NKENTSEU_TERMINAL_POSIX)
		// =====================================================================
		//  UNIX (macOS, Linux, BSD)
		// =====================================================================
		namespace {
			bool Executable(const NkString &chemin) {
				return !chemin.Empty() && ::access(chemin.CStr(), X_OK) == 0;
			}

			NkShellDecouvert Shell(const NkString &chemin, bool session) {
				NkShellDecouvert s;
				s.genre = NkTerminalGenreDuChemin(chemin.CStr());
				s.chemin = chemin;
				s.nom = Base(chemin);
				if (session)
					s.nom += " (session)";
				s.session = session;
#	if defined(__APPLE__)
				// macOS : Terminal.app ouvre des shells de LOGIN (le PATH de
				// Homebrew vient de /etc/zprofile). Sur un terminal, login = interactif.
				s.commande = chemin + " -l";
#	else
				s.commande = chemin + " -i";
#	endif
				return s;
			}
		} // namespace

		NkShellDecouvert NkTerminalShellDeRepli() {
			return Shell(NkString("/bin/sh"), true);
		}

		void NkTerminalListerWsl(NkVector<NkString> &distros) {
			distros.Clear();
		}

		void NkTerminalDecouvrirShells(NkVector<NkShellDecouvert> &out, bool avecWsl) {
			(void)avecWsl;
			out.Clear();
			NkVector<NkString> vus;
			auto ajouter = [&](const NkString &chemin, bool session) {
				if (!Executable(chemin))
					return;
				const NkString nom = Base(chemin);
				for (usize i = 0; i < vus.Size(); ++i)
					if (vus[i] == nom)
						return;
				vus.PushBack(nom);
				out.PushBack(Shell(chemin, session));
			};
			// 1. Le shell de la session : celui que l'utilisateur a choisi.
			if (const char *sh = std::getenv("SHELL"))
				ajouter(NkString(sh), true);
			// 2. /etc/shells, ou une liste connue s'il manque.
			NkVector<NkString> chemins;
			if (FILE *f = std::fopen("/etc/shells", "r")) {
				NkVector<char> texte;
				char b[512];
				usize n = 0;
				while ((n = std::fread(b, 1, sizeof(b), f)) > 0)
					for (usize i = 0; i < n; ++i)
						texte.PushBack(b[i]);
				std::fclose(f);
				texte.PushBack(0);
				NkTerminalLireEtcShells(texte.Data(), chemins);
			}
			if (chemins.Empty()) {
				const char *connus[] = {"/bin/bash", "/bin/zsh", "/usr/bin/fish", "/opt/homebrew/bin/fish",
										"/usr/local/bin/fish", "/bin/sh"};
				for (const char *c : connus)
					chemins.PushBack(NkString(c));
			}
			for (usize i = 0; i < chemins.Size(); ++i)
				ajouter(chemins[i], false);
			if (out.Empty())
				out.PushBack(NkTerminalShellDeRepli());
		}

#else
		// =====================================================================
		//  ANDROID, iOS, WEB : pas de shell systeme
		// =====================================================================
		NkShellDecouvert NkTerminalShellDeRepli() {
			return NkShellDecouvert{};
		}

		void NkTerminalListerWsl(NkVector<NkString> &distros) {
			distros.Clear();
		}

		void NkTerminalDecouvrirShells(NkVector<NkShellDecouvert> &out, bool avecWsl) {
			(void)avecWsl;
			out.Clear();
		}
#endif

	} // namespace editorkit
} // namespace nkentseu
