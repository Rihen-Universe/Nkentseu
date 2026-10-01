// =============================================================================
// NkTerminalPtyWin32.cpp — NkPty sous Windows : ConPTY (pseudo-console).
//   windows.h est CONFINE ici (voir l'en-tete). Repris de NKCode (NkPty.cpp)
//   avec deux corrections : STARTF_USESTDHANDLES et le Job « tuer a la
//   fermeture ». Les deux sont mesurees par le banc (NkTerminalProbe.h, t4/t6).
// =============================================================================
#include "NKEditorKit/Terminal/NkTerminalPty.h"

#if defined(NKENTSEU_TERMINAL_CONPTY)

#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"
#include "NKThreading/NkThread.h"

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

// PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE (peut manquer sur de vieux SDK).
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#	define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

namespace nkentseu {
	namespace editorkit {

		namespace {
			// Signatures ConPTY, chargees dynamiquement depuis kernel32 : un
			// Windows trop ancien (< 10 1809) refuse proprement au lieu de ne
			// pas demarrer du tout.
			typedef HRESULT(WINAPI *PFN_CreatePseudoConsole)(COORD, HANDLE, HANDLE, DWORD, void **);
			typedef HRESULT(WINAPI *PFN_ResizePseudoConsole)(void *, COORD);
			typedef void(WINAPI *PFN_ClosePseudoConsole)(void *);

			/// UTF-8 -> UTF-16 (tableau termine par 0).
			void VersLarge(const NkString &s, NkVector<wchar_t> &out) {
				out.Clear();
				const int n = MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, NULL, 0);
				if (n <= 0) {
					out.PushBack(0);
					return;
				}
				out.Resize(static_cast<usize>(n));
				if (MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, out.Data(), n) <= 0)
					out[0] = 0;
			}

			void Fermer(HANDLE &h) {
				if (h) {
					CloseHandle(h);
					h = NULL;
				}
			}
		} // namespace

		namespace {
			/// Les DESCENDANTS de `racine` (enfants, petits-enfants...), par
			/// l'instantane des processus. Sert quand le Job n'a pas pu etre pose.
			void Descendants(DWORD racine, NkVector<int64> &out) {
				HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
				if (snap == INVALID_HANDLE_VALUE)
					return;
				NkVector<DWORD> pids, parents;
				PROCESSENTRY32W pe;
				pe.dwSize = sizeof(pe);
				if (Process32FirstW(snap, &pe)) {
					do {
						pids.PushBack(pe.th32ProcessID);
						parents.PushBack(pe.th32ParentProcessID);
					} while (Process32NextW(snap, &pe));
				}
				CloseHandle(snap);
				NkVector<DWORD> file;
				file.PushBack(racine);
				for (usize k = 0; k < file.Size() && k < 256; ++k)
					for (usize i = 0; i < pids.Size(); ++i)
						if (parents[i] == file[k] && pids[i] != racine && pids[i] != 0) {
							bool deja = false;
							for (usize j = 0; j < file.Size(); ++j)
								deja = deja || file[j] == pids[i];
							if (!deja) {
								file.PushBack(pids[i]);
								out.PushBack(static_cast<int64>(pids[i]));
							}
						}
			}
		} // namespace

		struct NkPty::Impl {
				HANDLE inWrite = NULL;	// on ecrit dedans (entree du shell)
				HANDLE outRead = NULL;	// on lit dedans (sortie du shell)
				HANDLE process = NULL;	// le shell
				HANDLE job = NULL;		// le shell ET sa descendance
				void *pc = NULL;		// HPCON
				PFN_ResizePseudoConsole resize = nullptr;
				PFN_ClosePseudoConsole close = nullptr;
				threading::NkThread thread;
				threading::NkMutex mutex;
				NkVector<char> buf;	   // sortie brute (protegee par mutex)
				volatile bool lecture = false;

				void LireEnBoucle() {
					char tmp[4096];
					DWORD rd = 0;
					for (;;) {
						const BOOL ok = ReadFile(outRead, tmp, sizeof(tmp), &rd, NULL);
						if (!ok || rd == 0)
							break;
						threading::NkScopedLock<threading::NkMutex> lk(mutex);
						for (DWORD i = 0; i < rd; ++i)
							buf.PushBack(tmp[i]);
					}
					lecture = false;
				}
		};

		NkPty::NkPty() = default;

		NkPty::~NkPty() {
			Stop();
		}

		bool NkPty::Disponible() {
			HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
			return k32 && GetProcAddress(k32, "CreatePseudoConsole") != NULL;
		}

		const char *NkPty::NomMoteur() {
			return "ConPTY";
		}

		bool NkPty::Start(const NkString &cmdline, int16 cols, int16 rows, const NkString &cwd) {
			if (mImpl)
				return false; // deja lance : Stop() d'abord
			mErreur = NkString();
			mCols = cols < 1 ? 80 : cols;
			mRows = rows < 1 ? 24 : rows;

			HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
			PFN_CreatePseudoConsole pCreate =
				k32 ? (PFN_CreatePseudoConsole)GetProcAddress(k32, "CreatePseudoConsole") : nullptr;
			PFN_ResizePseudoConsole pResize =
				k32 ? (PFN_ResizePseudoConsole)GetProcAddress(k32, "ResizePseudoConsole") : nullptr;
			PFN_ClosePseudoConsole pClose = k32 ? (PFN_ClosePseudoConsole)GetProcAddress(k32, "ClosePseudoConsole") : nullptr;
			if (!pCreate || !pClose) {
				mErreur = "ConPTY indisponible : Windows 10 version 1809 ou plus recent est requis.";
				return false;
			}

			// Tubes : entree (inRead -> ConPTY, inWrite -> nous) ; sortie
			// (outRead -> nous, outWrite -> ConPTY). Non heritables.
			HANDLE inRead = NULL, inWrite = NULL, outRead = NULL, outWrite = NULL;
			if (!CreatePipe(&inRead, &inWrite, NULL, 0) || !CreatePipe(&outRead, &outWrite, NULL, 0)) {
				Fermer(inRead);
				Fermer(inWrite);
				mErreur = "Creation des tubes du terminal refusee par le systeme.";
				return false;
			}
			COORD taille;
			taille.X = mCols;
			taille.Y = mRows;
			void *hpc = NULL;
			const HRESULT hr = pCreate(taille, inRead, outWrite, 0, &hpc);
			// ConPTY a duplique ce dont il a besoin : nos copies ne servent plus.
			Fermer(inRead);
			Fermer(outWrite);
			if (FAILED(hr) || !hpc) {
				Fermer(inWrite);
				Fermer(outRead);
				mErreur = "CreatePseudoConsole a echoue.";
				return false;
			}

			Impl *im = new Impl();
			im->pc = hpc;
			im->inWrite = inWrite;
			im->outRead = outRead;
			im->resize = pResize;
			im->close = pClose;

			// STARTUPINFOEX + attribut PSEUDOCONSOLE.
			STARTUPINFOEXW si;
			ZeroMemory(&si, sizeof(si));
			si.StartupInfo.cb = sizeof(si);
			// ⚠️ LE DEFAUT N.1 DE L'ANCIEN MOTEUR. Sans ce drapeau, un client de
			//    pseudo-console HERITE des poignees standard du parent quand elles
			//    sont redirigees : l'invite partait dans la console de l'appelant
			//    et le terminal restait vide. Poignees NULLES + drapeau = le
			//    client n'a que la pseudo-console. (Mutation `stdhandles`.)
			if (!NkTerminalMutation("stdhandles")) {
				si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
				si.StartupInfo.hStdInput = NULL;
				si.StartupInfo.hStdOutput = NULL;
				si.StartupInfo.hStdError = NULL;
			}
			SIZE_T attrSize = 0;
			InitializeProcThreadAttributeList(NULL, 1, 0, &attrSize);
			si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrSize);
			const bool attrOk = si.lpAttributeList && InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrSize) &&
								UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, im->pc,
														  sizeof(im->pc), NULL, NULL);
			if (!attrOk) {
				if (si.lpAttributeList)
					HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
				pClose(im->pc);
				Fermer(im->inWrite);
				Fermer(im->outRead);
				delete im;
				mErreur = "Attribut de pseudo-console refuse.";
				return false;
			}

			NkVector<wchar_t> wcmd, wcwd;
			VersLarge(cmdline, wcmd);
			if (!cwd.Empty())
				VersLarge(cwd, wcwd);

			// ⚠️ LE DEFAUT N.2 : LA DESCENDANCE. Le shell est cree SUSPENDU, range
			//    dans un Job « tuer a la fermeture », puis relache : rien de ce
			//    qu'il lance ne peut naitre hors du Job. Fermer le Job tue l'arbre
			//    entier -- un `ping -t` compris. (Mutation `orphelins`.)
			const bool avecJob = !NkTerminalMutation("orphelins");
			if (avecJob) {
				im->job = CreateJobObjectW(NULL, NULL);
				if (im->job) {
					JOBOBJECT_EXTENDED_LIMIT_INFORMATION li;
					ZeroMemory(&li, sizeof(li));
					li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
					SetInformationJobObject(im->job, JobObjectExtendedLimitInformation, &li, sizeof(li));
				}
			}

			PROCESS_INFORMATION pi;
			ZeroMemory(&pi, sizeof(pi));
			const BOOL ok = CreateProcessW(NULL, wcmd.Data(), NULL, NULL, FALSE,
										   EXTENDED_STARTUPINFO_PRESENT | (im->job ? CREATE_SUSPENDED : 0), NULL,
										   wcwd.Empty() ? NULL : wcwd.Data(), &si.StartupInfo, &pi);
			const DWORD err = ok ? 0 : GetLastError();
			DeleteProcThreadAttributeList(si.lpAttributeList);
			HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
			if (!ok) {
				pClose(im->pc);
				Fermer(im->inWrite);
				Fermer(im->outRead);
				Fermer(im->job);
				delete im;
				mErreur = NkString("Impossible de lancer « ") + cmdline +
						  (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND
							   ? NkString(" » : programme introuvable.")
							   : (err == ERROR_DIRECTORY ? NkString(" » : dossier de depart invalide.")
														 : NkString(" » (erreur systeme).")));
				return false;
			}
			if (im->job && !AssignProcessToJobObject(im->job, pi.hProcess)) {
				// Un Job imbrique refuse (vieux Windows, parent deja dans un Job
				// sans « breakaway ») : on continue SANS, et l'arret retombe sur
				// TerminateProcess -- le comportement d'avant, pas pire.
				Fermer(im->job);
			}
			if (pi.hThread) {
				ResumeThread(pi.hThread);
				CloseHandle(pi.hThread);
			}
			im->process = pi.hProcess;
			mPid = static_cast<int64>(pi.dwProcessId);
			im->lecture = true;
			mImpl = im;
			im->thread = threading::NkThread([im](void *) { im->LireEnBoucle(); });
			return true;
		}

		void NkPty::Write(const char *data, usize len) {
			if (!mImpl || !mImpl->inWrite || !data || len == 0)
				return;
			usize fait = 0;
			while (fait < len) {
				DWORD wr = 0;
				if (!WriteFile(mImpl->inWrite, data + fait, static_cast<DWORD>(len - fait), &wr, NULL) || wr == 0)
					break;
				fait += wr;
			}
		}

		void NkPty::Resize(int16 cols, int16 rows) {
			if (!mImpl || !mImpl->pc || !mImpl->resize)
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
				return; // contre-epreuve : la taille du shell ne suit plus
			COORD c;
			c.X = cols;
			c.Y = rows;
			mImpl->resize(mImpl->pc, c);
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
			// ⚠️ PAS « le fil de lecture tourne » : la pseudo-console garde son
			//    tube ouvert apres un `exit` du shell. C'est le PROCESSUS qu'on
			//    interroge, sinon un shell termine passait pour vivant.
			return mImpl && mImpl->process && WaitForSingleObject(mImpl->process, 0) == WAIT_TIMEOUT;
		}

		int32 NkPty::CodeSortie() const {
			if (!mImpl || !mImpl->process)
				return -1;
			DWORD code = 0;
			if (!GetExitCodeProcess(mImpl->process, &code) || code == STILL_ACTIVE)
				return -1;
			return static_cast<int32>(code);
		}

		void NkPty::Stop() {
			Impl *im = mImpl;
			if (!im)
				return;
			// 1) EOF sur l'entree du shell.
			Fermer(im->inWrite);
			// 2) L'ARBRE ENTIER d'abord (Job), puis le shell seul s'il n'a pas pu
			//    y entrer. Tuer AVANT de fermer la pseudo-console : sur de vieux
			//    Windows 10, ClosePseudoConsole attend que ses clients sortent.
			if (im->job) {
				TerminateJobObject(im->job, 0);
			} else if (!NkTerminalMutation("orphelins") && mPid > 0) {
				// Pas de Job (Windows ancien, Job imbrique refuse) : on descend
				// l'arbre a la main. Moins sur (un processus ne entre l'instantane
				// et l'arret echappe), mais jamais pire que l'ancien moteur.
				NkVector<int64> d;
				Descendants(static_cast<DWORD>(mPid), d);
				for (usize i = 0; i < d.Size(); ++i)
					Tuer(d[i]);
			}
			if (im->process)
				TerminateProcess(im->process, 0);
			// 3) Fermer la pseudo-console : le tube de sortie se ferme, ReadFile
			//    rend la main, le fil de lecture sort.
			if (im->pc && im->close) {
				im->close(im->pc);
				im->pc = NULL;
			}
			if (im->thread.Joinable())
				im->thread.Join();
			Fermer(im->outRead);
			// 4) Attendre la mort reelle : apres Stop, ProcessusVivant(Pid()) est
			//    FAUX. Un banc qui verifie juste apres ne doit pas tomber sur un
			//    processus encore en train de mourir.
			if (im->process) {
				WaitForSingleObject(im->process, 3000);
				Fermer(im->process);
			}
			Fermer(im->job); // KILL_ON_JOB_CLOSE : derniere garantie
			delete im;
			mImpl = nullptr;
		}

		void NkPty::ProcessusDuGroupe(NkVector<int64> &pids) const {
			pids.Clear();
			if (!mImpl)
				return;
			if (!mImpl->job) {
				if (mPid > 0) {
					pids.PushBack(mPid);
					Descendants(static_cast<DWORD>(mPid), pids);
				}
				return;
			}
			// Liste a taille fixe : 64 processus suffisent largement a un shell
			// interactif ; au-dela, la liste est TRONQUEE, pas fausse.
			struct {
					JOBOBJECT_BASIC_PROCESS_ID_LIST l;
					ULONG_PTR reste[63];
			} lst;
			ZeroMemory(&lst, sizeof(lst));
			lst.l.NumberOfAssignedProcesses = 64;
			if (!QueryInformationJobObject(mImpl->job, JobObjectBasicProcessIdList, &lst, sizeof(lst), NULL))
				return;
			for (DWORD i = 0; i < lst.l.NumberOfProcessIdsInList && i < 64; ++i)
				pids.PushBack(static_cast<int64>(lst.l.ProcessIdList[i]));
		}

		void NkPty::Tuer(int64 pid) {
			if (pid <= 0)
				return;
			if (HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid))) {
				TerminateProcess(h, 1);
				WaitForSingleObject(h, 2000);
				CloseHandle(h);
			}
		}

		bool NkPty::ProcessusVivant(int64 pid) {
			if (pid <= 0)
				return false;
			HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
			if (!h)
				return GetLastError() == ERROR_ACCESS_DENIED; // existe, mais pas a nous
			const bool vivant = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
			CloseHandle(h);
			return vivant;
		}

	} // namespace editorkit
} // namespace nkentseu

#endif // NKENTSEU_TERMINAL_CONPTY
