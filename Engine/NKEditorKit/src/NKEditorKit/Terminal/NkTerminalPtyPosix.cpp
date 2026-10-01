// =============================================================================
// NkTerminalPtyPosix.cpp — NkPty sous macOS, Linux et BSD : un pseudo-terminal
// POSIX (posix_openpt / grantpt / unlockpt / ptsname) et un fork du shell.
//
// POURQUOI posix_openpt ET PAS forkpty : forkpty vit dans libutil sous Linux
// (lier `util`), dans util.h sous macOS, et n'existe pas partout. posix_openpt
// est la porte NORMALISEE ; elle n'ajoute aucune bibliotheque aux douze
// applications qui lient le kit. Le resultat est le meme : un vrai terminal,
// avec invite, couleurs, historique, controle de travaux et redimensionnement.
//
// CE QUI EST FAIT DANS L'ENFANT, ET RIEN D'AUTRE : entre fork et execve, seules
// des fonctions sures en contexte de signal (setsid, open, ioctl, dup2, chdir,
// sigaction, execve). Arguments, environnement et chemin du programme sont
// PREPARES AVANT le fork : l'application a plusieurs fils, et un malloc dans
// l'enfant peut s'y bloquer pour toujours sur un verrou orphelin.
// =============================================================================
#include "NKEditorKit/Terminal/NkTerminalPty.h"

#if defined(NKENTSEU_TERMINAL_POSIX)

#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"
#include "NKThreading/NkThread.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#if defined(__APPLE__)
#	include <sys/sysctl.h>
#endif

extern char **environ;

namespace nkentseu {
	namespace editorkit {

		namespace {
			/// Le programme `nom` dans le PATH (« bash » -> « /bin/bash »). Vide si
			/// introuvable. Un nom qui contient « / » est rendu tel quel.
			NkString Trouver(const NkString &nom) {
				if (nom.Empty())
					return NkString();
				for (usize i = 0; i < nom.Size(); ++i)
					if (nom.CStr()[i] == '/')
						return nom;
				const char *path = std::getenv("PATH");
				if (!path || !*path)
					path = "/usr/local/bin:/usr/bin:/bin";
				NkString dossier;
				for (const char *p = path;; ++p) {
					if (*p == ':' || *p == 0) {
						const NkString cand = (dossier.Empty() ? NkString(".") : dossier) + "/" + nom;
						if (::access(cand.CStr(), X_OK) == 0)
							return cand;
						dossier = NkString();
						if (*p == 0)
							break;
						continue;
					}
					dossier += *p;
				}
				return NkString();
			}

			/// Vrai si `nom=` commence la variable `var` (« TERM=xterm »).
			bool EstVariable(const char *var, const char *nom) {
				const usize n = std::strlen(nom);
				return std::strncmp(var, nom, n) == 0 && var[n] == '=';
			}

			void AttendreUnPeu() {
				struct timespec ts;
				ts.tv_sec = 0;
				ts.tv_nsec = 10 * 1000 * 1000; // 10 ms
				nanosleep(&ts, nullptr);
			}
		} // namespace

		struct NkPty::Impl {
				int maitre = -1;
				pid_t pid = -1;
				char esclave[128] = {};
				threading::NkThread thread;
				threading::NkMutex mutex;
				NkVector<char> buf;
				volatile bool arret = false;   // Stop() demande la fin du fil
				volatile bool termine = false; // le shell est sorti (moissonne)
				volatile int code = -1;

				/// Moissonne le shell s'il est sorti. Sans cela il resterait ZOMBIE.
				void Moissonner(bool bloquer) {
					if (pid <= 0 || termine)
						return;
					int st = 0;
					const pid_t r = ::waitpid(pid, &st, bloquer ? 0 : WNOHANG);
					if (r == pid) {
						code = WIFEXITED(st) ? WEXITSTATUS(st) : (WIFSIGNALED(st) ? 128 + WTERMSIG(st) : -1);
						termine = true;
					}
				}

				// ⚠️ LECTURE NON BLOQUANTE + poll a 50 ms. L'ancienne boucle faisait
				//    un read() BLOQUANT et comptait sur close() pour le debloquer :
				//    sous Linux, fermer un descripteur ne reveille PAS un read() en
				//    cours dans un autre fil. Si le shell ignorait SIGHUP, Stop()
				//    attendait le fil pour toujours.
				void LireEnBoucle() {
					char tmp[4096];
					while (!arret) {
						struct pollfd pf;
						pf.fd = maitre;
						pf.events = POLLIN;
						pf.revents = 0;
						const int r = ::poll(&pf, 1, 50);
						if (r < 0) {
							if (errno == EINTR)
								continue;
							break;
						}
						if (r == 0) {
							Moissonner(false);
							continue;
						}
						bool fin = false;
						for (;;) {
							const ssize_t n = ::read(maitre, tmp, sizeof(tmp));
							if (n > 0) {
								threading::NkScopedLock<threading::NkMutex> lk(mutex);
								for (ssize_t i = 0; i < n; ++i)
									buf.PushBack(tmp[i]);
								continue;
							}
							if (n < 0 && errno == EINTR)
								continue;
							if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
								break;
							fin = true; // 0 ou EIO : plus aucun esclave ouvert
							break;
						}
						if (fin || (pf.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0) {
							Moissonner(false);
							if (fin || (pf.revents & POLLNVAL) != 0)
								break;
						}
					}
					Moissonner(false);
				}
		};

		NkPty::NkPty() = default;

		NkPty::~NkPty() {
			Stop();
		}

		bool NkPty::Disponible() {
			return true;
		}

		const char *NkPty::NomMoteur() {
			return "POSIX (posix_openpt)";
		}

		bool NkPty::Start(const NkString &cmdline, int16 cols, int16 rows, const NkString &cwd) {
			if (mImpl)
				return false;
			mErreur = NkString();
			mCols = cols < 1 ? 80 : cols;
			mRows = rows < 1 ? 24 : rows;

			// ── 1. TOUT CE QUI ALLOUE, AVANT LE FORK ──────────────────────────
			NkVector<NkString> args;
			NkTerminalDecouperCommande(cmdline.CStr(), args);
			if (args.Empty()) {
				mErreur = "Commande de shell vide.";
				return false;
			}
			const NkString programme = Trouver(args[0]);
			if (programme.Empty()) {
				mErreur = NkString("Programme introuvable : ") + args[0];
				return false;
			}
			NkVector<char *> argv;
			for (usize i = 0; i < args.Size(); ++i)
				argv.PushBack(const_cast<char *>(args[i].CStr()));
			argv.PushBack(nullptr);

			// L'environnement : celui de l'application, avec TERM et COLORTERM
			// remplaces. Sans TERM, la plupart des shells n'emettent ni couleurs
			// ni sequences de positionnement.
			NkVector<NkString> ajouts;
			ajouts.PushBack(NkString("TERM=xterm-256color"));
			ajouts.PushBack(NkString("COLORTERM=truecolor"));
			ajouts.PushBack(NkString("TERM_PROGRAM=Nkentseu"));
			NkVector<char *> envp;
			for (char **e = environ; e && *e; ++e)
				if (!EstVariable(*e, "TERM") && !EstVariable(*e, "COLORTERM") && !EstVariable(*e, "TERM_PROGRAM"))
					envp.PushBack(*e);
			for (usize i = 0; i < ajouts.Size(); ++i)
				envp.PushBack(const_cast<char *>(ajouts[i].CStr()));
			envp.PushBack(nullptr);

			// ── 2. LE PSEUDO-TERMINAL ─────────────────────────────────────────
			const int maitre = ::posix_openpt(O_RDWR | O_NOCTTY);
			if (maitre < 0 || ::grantpt(maitre) != 0 || ::unlockpt(maitre) != 0) {
				if (maitre >= 0)
					::close(maitre);
				mErreur = "Pseudo-terminal refuse par le systeme (posix_openpt).";
				return false;
			}
			const char *nomEsclave = ::ptsname(maitre);
			if (!nomEsclave) {
				::close(maitre);
				mErreur = "Pseudo-terminal sans esclave (ptsname).";
				return false;
			}
			Impl *im = new Impl();
			im->maitre = maitre;
			std::snprintf(im->esclave, sizeof(im->esclave), "%s", nomEsclave);
			struct winsize ws;
			std::memset(&ws, 0, sizeof(ws));
			ws.ws_col = static_cast<unsigned short>(mCols);
			ws.ws_row = static_cast<unsigned short>(mRows);
			(void)::ioctl(maitre, TIOCSWINSZ, &ws);

			const char *dossier = cwd.Empty() ? nullptr : cwd.CStr();
			const pid_t pid = ::fork();
			if (pid < 0) {
				::close(maitre);
				delete im;
				mErreur = "fork() a echoue.";
				return false;
			}
			if (pid == 0) {
				// ── ENFANT ── fonctions sures en contexte de signal uniquement.
				::setsid(); // nouvelle SESSION : le shell en est le chef
				const int esclave = ::open(im->esclave, O_RDWR);
				if (esclave < 0)
					::_exit(126);
#if defined(TIOCSCTTY)
				(void)::ioctl(esclave, TIOCSCTTY, 0); // terminal de controle (BSD, macOS)
#endif
				(void)::ioctl(esclave, TIOCSWINSZ, &ws);
				::dup2(esclave, 0);
				::dup2(esclave, 1);
				::dup2(esclave, 2);
				if (esclave > 2)
					::close(esclave);
				::close(maitre);
				if (dossier)
					(void)::chdir(dossier); // un echec n'est pas bloquant
				// L'application peut ignorer SIGPIPE ou bloquer des signaux : un
				// signal IGNORE le reste a travers execve, et un `ls | head` du
				// shell ne finirait plus. On rend les dispositions par defaut.
				struct sigaction sa;
				std::memset(&sa, 0, sizeof(sa));
				sa.sa_handler = SIG_DFL;
				const int sigs[] = {SIGPIPE, SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGCHLD, SIGTSTP, SIGTTIN, SIGTTOU, SIGWINCH};
				for (int s : sigs)
					::sigaction(s, &sa, nullptr);
				sigset_t vide;
				sigemptyset(&vide);
				::sigprocmask(SIG_SETMASK, &vide, nullptr);
				::execve(programme.CStr(), argv.Data(), envp.Data());
				::_exit(127);
			}

			// ── PARENT ────────────────────────────────────────────────────────
			// Un seul descripteur pour lire ET ecrire, non bloquant.
			const int fl = ::fcntl(maitre, F_GETFL, 0);
			(void)::fcntl(maitre, F_SETFL, fl | O_NONBLOCK);
			(void)::fcntl(maitre, F_SETFD, FD_CLOEXEC);
			im->pid = pid;
			mPid = static_cast<int64>(pid);
			mImpl = im;
			im->thread = threading::NkThread([im](void *) { im->LireEnBoucle(); });
			return true;
		}

		void NkPty::Write(const char *data, usize len) {
			if (!mImpl || mImpl->maitre < 0 || !data || len == 0)
				return;
			// Ecriture PARTIELLE possible (descripteur non bloquant, tampon du
			// terminal plein pendant un gros collage) : on boucle, en cedant la
			// main sur EAGAIN, sinon le collage se perdrait en silence.
			usize fait = 0;
			int essais = 0;
			while (fait < len && essais < 500) {
				const ssize_t wr = ::write(mImpl->maitre, data + fait, len - fait);
				if (wr > 0) {
					fait += static_cast<usize>(wr);
					continue;
				}
				if (wr < 0 && errno == EINTR)
					continue;
				if (wr < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
					++essais;
					AttendreUnPeu();
					continue;
				}
				break;
			}
		}

		void NkPty::Resize(int16 cols, int16 rows) {
			if (!mImpl || mImpl->maitre < 0)
				return;
			if (cols < 1)
				cols = 1;
			if (rows < 1)
				rows = 1;
			if (cols == mCols && rows == mRows)
				return;
			mCols = cols;
			mRows = rows;
			if (NkTerminalMutation("resize"))
				return;
			struct winsize ws;
			std::memset(&ws, 0, sizeof(ws));
			ws.ws_col = static_cast<unsigned short>(cols);
			ws.ws_row = static_cast<unsigned short>(rows);
			// TIOCSWINSZ : le noyau range la taille ET envoie SIGWINCH au groupe
			// de premier plan. Sans cela, vim ou htop gardent l'ancienne taille.
			(void)::ioctl(mImpl->maitre, TIOCSWINSZ, &ws);
		}

		void NkPty::Drain(NkVector<char> &out) {
			if (!mImpl)
				return;
			threading::NkScopedLock<threading::NkMutex> lk(mImpl->mutex);
			for (usize i = 0; i < mImpl->buf.Size(); ++i)
				out.PushBack(mImpl->buf[i]);
			mImpl->buf.Clear();
		}

		bool NkPty::Running() const {
			return mImpl && mImpl->pid > 0 && !mImpl->termine;
		}

		int32 NkPty::CodeSortie() const {
			return (mImpl && mImpl->termine) ? static_cast<int32>(mImpl->code) : -1;
		}

		void NkPty::ProcessusDuGroupe(NkVector<int64> &pids) const {
			pids.Clear();
			if (!mImpl || mImpl->pid <= 0)
				return;
			const pid_t sid = mImpl->pid; // setsid() : la session porte le PID du shell
			pids.PushBack(static_cast<int64>(sid));
#if defined(__linux__)
			// Linux : /proc/<pid>/stat, 6e champ = la session. Le nom du
			// programme (2e champ) est entre parentheses et peut contenir des
			// espaces : on repart de la DERNIERE parenthese fermante.
			DIR *d = ::opendir("/proc");
			if (!d)
				return;
			while (struct dirent *e = ::readdir(d)) {
				char *fin = nullptr;
				const long p = std::strtol(e->d_name, &fin, 10);
				if (!fin || *fin != 0 || p <= 0 || p == sid)
					continue;
				char chemin[64], ligne[512];
				std::snprintf(chemin, sizeof(chemin), "/proc/%ld/stat", p);
				FILE *f = std::fopen(chemin, "r");
				if (!f)
					continue;
				const usize n = std::fread(ligne, 1, sizeof(ligne) - 1, f);
				std::fclose(f);
				ligne[n] = 0;
				const char *q = std::strrchr(ligne, ')');
				char etat = 0;
				int ppid = 0, pgrp = 0, session = 0;
				if (q && std::sscanf(q + 1, " %c %d %d %d", &etat, &ppid, &pgrp, &session) == 4 && session == sid)
					pids.PushBack(static_cast<int64>(p));
			}
			::closedir(d);
#elif defined(__APPLE__)
			// macOS : les processus dont le terminal de CONTROLE est notre
			// esclave (KERN_PROC_TTY). Un travail en arriere-plan (« sleep 30 & »)
			// a son propre groupe mais garde ce terminal.
			struct stat st;
			if (::stat(mImpl->esclave, &st) != 0)
				return;
			int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_TTY, static_cast<int>(st.st_rdev)};
			size_t taille = 0;
			if (::sysctl(mib, 4, nullptr, &taille, nullptr, 0) != 0 || taille == 0)
				return;
			NkVector<char> tampon;
			tampon.Resize(taille + 16 * sizeof(struct kinfo_proc));
			taille = tampon.Size();
			if (::sysctl(mib, 4, tampon.Data(), &taille, nullptr, 0) != 0)
				return;
			const struct kinfo_proc *kp = reinterpret_cast<const struct kinfo_proc *>(tampon.Data());
			const usize nb = taille / sizeof(struct kinfo_proc);
			for (usize i = 0; i < nb; ++i)
				if (kp[i].kp_proc.p_pid != sid)
					pids.PushBack(static_cast<int64>(kp[i].kp_proc.p_pid));
#endif
		}

		void NkPty::Stop() {
			Impl *im = mImpl;
			if (!im)
				return;
			if (im->pid > 0 && !im->termine) {
				// 1) Recenser la descendance TANT QUE le shell vit : une fois le
				//    chef de session mort, le terminal de controle est retire et
				//    on ne la retrouverait plus.
				NkVector<int64> groupe;
				if (!NkTerminalMutation("orphelins"))
					ProcessusDuGroupe(groupe);
				// 2) SIGHUP : « le terminal a disparu », ce qu'un vrai emulateur
				//    envoie en se fermant. Au groupe du shell, puis a chacun.
				(void)::kill(-im->pid, SIGHUP);
				for (usize i = 0; i < groupe.Size(); ++i)
					(void)::kill(static_cast<pid_t>(groupe[i]), SIGHUP);
				// 3) 300 ms pour sortir proprement.
				for (int k = 0; k < 30 && !im->termine; ++k) {
					im->Moissonner(false);
					if (!im->termine)
						AttendreUnPeu();
				}
				// 4) Ce qui reste est tue, descendance comprise.
				if (!NkTerminalMutation("orphelins")) {
					for (usize i = 0; i < groupe.Size(); ++i)
						(void)::kill(static_cast<pid_t>(groupe[i]), SIGKILL);
				}
				if (!im->termine) {
					(void)::kill(-im->pid, SIGKILL);
					(void)::kill(im->pid, SIGKILL);
					im->Moissonner(true);
				}
			}
			im->arret = true;
			if (im->thread.Joinable())
				im->thread.Join();
			if (im->maitre >= 0) {
				::close(im->maitre);
				im->maitre = -1;
			}
			delete im;
			mImpl = nullptr;
		}

		void NkPty::Tuer(int64 pid) {
			if (pid > 0)
				(void)::kill(static_cast<pid_t>(pid), SIGKILL);
		}

		bool NkPty::ProcessusVivant(int64 pid) {
			if (pid <= 0)
				return false;
			if (::kill(static_cast<pid_t>(pid), 0) != 0 && errno != EPERM)
				return false;
#if defined(__linux__)
			// Un ZOMBIE repond a kill(pid, 0) : il n'est pourtant plus vivant.
			char chemin[64], ligne[256];
			std::snprintf(chemin, sizeof(chemin), "/proc/%lld/stat", static_cast<long long>(pid));
			if (FILE *f = std::fopen(chemin, "r")) {
				const usize n = std::fread(ligne, 1, sizeof(ligne) - 1, f);
				std::fclose(f);
				ligne[n] = 0;
				const char *q = std::strrchr(ligne, ')');
				if (q && q[1] == ' ' && (q[2] == 'Z' || q[2] == 'X'))
					return false;
			}
#elif defined(__APPLE__)
			int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
			struct kinfo_proc kp;
			size_t taille = sizeof(kp);
			if (::sysctl(mib, 4, &kp, &taille, nullptr, 0) == 0 && taille > 0 && kp.kp_proc.p_stat == SZOMB)
				return false;
#endif
			return true;
		}

	} // namespace editorkit
} // namespace nkentseu

#endif // NKENTSEU_TERMINAL_POSIX
