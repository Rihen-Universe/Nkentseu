// =============================================================================
// NkEmbeddedJenga.cpp — implementation pybind11 (isolee dans cette TU : les
//   en-tetes Python/pybind11 sont lourds et ne doivent pas fuir dans main.cpp).
//   Voir NkEmbeddedJenga.h pour le contrat.
// =============================================================================
#include "NKCode/Project/NkEmbeddedJenga.h"
#include "NKCode/Project/NkProcess.h" // NkStripAnsiInto (transcript sans codes couleur)
#include "NKFileSystem/NkDirectory.h"

// En config Debug, _DEBUG ferait basculer pyconfig.h sur l'ABI DEBUG de
// CPython (Py_DEBUG/Py_REF_DEBUG -> symboles _Py_INCREF_IncRefTotal...)
// absente du python312.dll RELEASE embarque. pybind11 n'applique ce
// contournement que sous _MSC_VER — on le fait ici pour clang-mingw.
#if defined(_DEBUG)
#	undef _DEBUG
#endif
#include <pybind11/embed.h>
#include <pybind11/functional.h>

#if defined(_WIN32)
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	include <windows.h>
#else
#	include <unistd.h>
#endif

#include <cstdlib>
#include <string>

namespace {
	void NkSleepMs(unsigned ms) {
#if defined(_WIN32)
		::Sleep(ms);
#else
		::usleep(ms * 1000u);
#endif
	}
} // namespace

namespace py = pybind11;

namespace nkentseu {
	namespace nkcode {

		namespace {
			// Chemins resolus par Configure() — proteges par le fait qu'ils sont
			// ecrits UNE fois au demarrage, avant tout Start().
			NkString gPyHome;	 // dossier du runtime embarque (python312.dll, zip, DLLs)
			NkString gJengaSrc;	 // dossier contenant le package Jenga/ (tools/jenga-src)
			bool gConfigured = false;

			std::wstring WidenUtf8(const NkString &s) {
#if defined(_WIN32)
				if (s.Empty())
					return std::wstring();
				const int n = MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, nullptr, 0);
				std::wstring w(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
				if (n > 1)
					MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, &w[0], n);
				return w;
#else
				std::wstring w;
				for (const char *p = s.CStr(); *p; ++p)
					w += static_cast<wchar_t>(*p); // ASCII-only fallback (chemins) — Linux/macOS: Phase 6
				return w;
#endif
			}

			bool DirExists(const NkString &p) {
				return !p.Empty() && NkDirectory::Exists(p.CStr());
			}
		} // namespace

		NkEmbeddedJenga &NkEmbeddedJenga::Get() {
			static NkEmbeddedJenga inst;
			return inst;
		}

		void NkEmbeddedJenga::Configure(const NkString &exeDir) {
			// Prod : tools/ a cote de NKCode.exe (pose par le packaging, Phase 5).
			// Dev  : repli sur le PythonEmbed vendorise du repo (exe dans
			// Build/Bin/<cfg>-<os>/NKCode -> racine repo = 4 niveaux au-dessus) et
			// sur NKCODE_JENGA_SRC pour les sources Jenga (repo Jenga local).
			gPyHome = exeDir + "/tools/python-embed";
			if (!DirExists(gPyHome)) {
				const NkString devRoot = exeDir + "/../../../..";
				const NkString dev = devRoot + "/Externals/Libs/PythonEmbed/runtime";
				if (DirExists(dev))
					gPyHome = dev;
			}
			gJengaSrc = exeDir + "/tools/jenga-src";
			if (!DirExists(gJengaSrc + "/Jenga")) {
				const char *env = std::getenv("NKCODE_JENGA_SRC");
				if (env && *env && DirExists(NkString(env) + "/Jenga"))
					gJengaSrc = env;
			}
			gConfigured = true;
		}

		bool NkEmbeddedJenga::Available() {
			return gConfigured && DirExists(gPyHome) && DirExists(gJengaSrc + "/Jenga");
		}

		bool NkEmbeddedJenga::Start(const Request &req) {
			threading::NkScopedLock<NkMutex> lk(mMutex);
			if (mRunning || mHasPending)
				return false;
			mPending = req;
			mHasPending = true;
			mRunning = true; // considere "en cours" des le depot (meme contrat que NkProcess)
			mDone = false;
			if (!mWorkerStarted) {
				mWorkerStarted = true;
				mThread = NkThread([this](void *) { WorkerMain(); });
			}
			return true;
		}

		bool NkEmbeddedJenga::Running() const {
			threading::NkScopedLock<NkMutex> lk(mMutex);
			return mRunning;
		}

		bool NkEmbeddedJenga::Done() const {
			threading::NkScopedLock<NkMutex> lk(mMutex);
			return mDone;
		}

		int NkEmbeddedJenga::ExitCode() const {
			threading::NkScopedLock<NkMutex> lk(mMutex);
			return mExit;
		}

		void NkEmbeddedJenga::Drain(NkVector<NkString> &outLines, NkVector<NkJengaProgressEvent> &outEvents) {
			threading::NkScopedLock<NkMutex> lk(mMutex);
			for (usize i = 0; i < mLines.Size(); ++i)
				outLines.PushBack(mLines[i]);
			mLines.Clear();
			for (usize i = 0; i < mEvents.Size(); ++i)
				outEvents.PushBack(mEvents[i]);
			mEvents.Clear();
		}

		void NkEmbeddedJenga::Shutdown() {
			NkEmbeddedJenga &j = Get();
			{
				threading::NkScopedLock<NkMutex> lk(j.mMutex);
				j.mQuit = true;
			}
			if (j.mThread.Joinable())
				j.mThread.Join();
		}

		void NkEmbeddedJenga::PushLine(const NkString &s) {
			threading::NkScopedLock<NkMutex> lk(mMutex);
			mLines.PushBack(s);
		}

		void NkEmbeddedJenga::PushEvent(const NkJengaProgressEvent &e) {
			threading::NkScopedLock<NkMutex> lk(mMutex);
			mEvents.PushBack(e);
		}

		void NkEmbeddedJenga::WorkerMain() {
			// ── Interpreteur ISOLE, init/finalize SUR CE thread (exigence CPython).
			// isolated=1 : ignore PYTHONHOME/PYTHONPATH/site utilisateur — AUCUNE
			// contamination par un Python systeme, c'est tout l'interet. ──
			PyConfig config;
			PyConfig_InitIsolatedConfig(&config);
			const std::wstring home = WidenUtf8(gPyHome);
			PyConfig_SetString(&config, &config.home, home.c_str());
			config.site_import = 0;
			config.module_search_paths_set = 1;
			const std::wstring zip = WidenUtf8(gPyHome + "/python312.zip");
			const std::wstring dlls = WidenUtf8(gPyHome + "/DLLs");
			const std::wstring src = WidenUtf8(gJengaSrc);
			PyWideStringList_Append(&config.module_search_paths, zip.c_str());
			PyWideStringList_Append(&config.module_search_paths, home.c_str());
			PyWideStringList_Append(&config.module_search_paths, dlls.c_str());
			PyWideStringList_Append(&config.module_search_paths, src.c_str());

			try {
				py::scoped_interpreter guard{&config};

				for (;;) {
					Request req;
					bool quit = false, has = false;
					{
						// NkScopedLock est RAII pur (pas d'Unlock manuel) : on copie
						// l'etat sous verrou puis on agit HORS verrou.
						threading::NkScopedLock<NkMutex> lk(mMutex);
						quit = mQuit;
						has = mHasPending;
						if (has) {
							req = mPending;
							mHasPending = false;
						}
					}
					if (quit)
						break;
					if (!has) {
						// pas de condvar necessaire : les builds durent des secondes,
						// un poll court est invisible et garde ce code trivial.
						NkSleepMs(30);
						continue;
					}

					int exitCode = 1;
					try {
						py::module_ embed = py::module_::import("Jenga.Core.Embed");

						// Sink duck-type : SimpleNamespace + cpp_function — pas besoin
						// d'enregistrer un type pybind (PYBIND11_EMBEDDED_MODULE inutile).
						py::object ns = py::module_::import("types").attr("SimpleNamespace")();
						ns.attr("OnProjectTotal") = py::cpp_function([this](int total) {
							NkJengaProgressEvent e;
							e.kind = NkJengaProgressEvent::PROJECT_TOTAL;
							e.total = total;
							PushEvent(e);
						});
						ns.attr("OnProjectDone") = py::cpp_function([this](bool okv) {
							NkJengaProgressEvent e;
							e.kind = NkJengaProgressEvent::PROJECT_DONE;
							e.ok = okv;
							PushEvent(e);
						});
						ns.attr("OnFileTotal") = py::cpp_function([this](std::string proj, int total) {
							NkJengaProgressEvent e;
							e.kind = NkJengaProgressEvent::FILE_TOTAL;
							e.project = proj.c_str();
							e.total = total;
							PushEvent(e);
						});
						ns.attr("OnFileDone") = py::cpp_function(
							[this](std::string proj, int index, int total, std::string file, bool okv, bool warned) {
								NkJengaProgressEvent e;
								e.kind = NkJengaProgressEvent::FILE_DONE;
								e.project = proj.c_str();
								e.file = file.c_str();
								e.index = index;
								e.total = total;
								e.ok = okv;
								e.warned = warned;
								PushEvent(e);
							});
						ns.attr("OnCompileError") =
							py::cpp_function([this](std::string proj, std::string file, std::string msg) {
								NkJengaProgressEvent e;
								e.kind = NkJengaProgressEvent::COMPILE_ERROR;
								e.project = proj.c_str();
								e.file = file.c_str();
								e.message = msg.c_str();
								PushEvent(e);
							});
						ns.attr("OnLinkError") =
							py::cpp_function([this](std::string proj, std::string file, std::string msg) {
								NkJengaProgressEvent e;
								e.kind = NkJengaProgressEvent::LINK_ERROR;
								e.project = proj.c_str();
								e.file = file.c_str();
								e.message = msg.c_str();
								PushEvent(e);
							});
						ns.attr("OnLogLine") = py::cpp_function([this](std::string line) {
							char clean[8192]; // meme nettoyage ANSI que NkProcess (panneau Sortie)
							NkStripAnsiInto(line.c_str(), clean, sizeof(clean));
							PushLine(NkString(clean));
						});

						const char *fn = (req.kind == "rebuild") ? "Rebuild" : "Build";
						py::object res = embed.attr(fn)(
							py::arg("jenga_file") = (req.jengaFile.Empty() ? py::object(py::none())
																		   : py::object(py::str(req.jengaFile.CStr()))),
							py::arg("target") = (req.target.Empty() ? py::object(py::none())
																	 : py::object(py::str(req.target.CStr()))),
							py::arg("config") = py::str(req.config.Empty() ? "Debug" : req.config.CStr()),
							py::arg("platform") = (req.platform.Empty() ? py::object(py::none())
																		 : py::object(py::str(req.platform.CStr()))),
							py::arg("toolchain") = (req.toolchain.Empty() ? py::object(py::none())
																		   : py::object(py::str(req.toolchain.CStr()))),
							py::arg("sink") = ns);
						exitCode = res.attr("exitCode").cast<int>();
						const std::string errMsg = res.attr("errorMessage").cast<std::string>();
						if (!errMsg.empty())
							PushLine(NkString("[jenga-embed] erreur: ") + errMsg.c_str());
					} catch (const std::exception &ex) {
						PushLine(NkString("[jenga-embed] exception: ") + ex.what());
						exitCode = 1;
					}

					{
						threading::NkScopedLock<NkMutex> lk(mMutex);
						mExit = exitCode;
						mRunning = false;
						mDone = true;
					}
				}
				// La destruction de `guard` (Py_FinalizeEx) se fait ICI, sur ce thread.
			} catch (const std::exception &ex) {
				// Interpreteur impossible a initialiser (runtime absent/corrompu).
				PushLine(NkString("[jenga-embed] init interpreteur impossible: ") + ex.what());
				threading::NkScopedLock<NkMutex> lk(mMutex);
				mExit = 1;
				mRunning = false;
				mDone = true;
			}
			PyConfig_Clear(&config);
		}

	} // namespace nkcode
} // namespace nkentseu
