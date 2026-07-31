// =============================================================================
// NkEmbeddedJenga.cpp — implementation pybind11 (isolee dans cette TU : les
//   en-tetes Python/pybind11 sont lourds et ne doivent pas fuir dans main.cpp).
//   Voir NkEmbeddedJenga.h pour le contrat.
// =============================================================================
#include "NKCode/Project/NkEmbeddedJenga.h"

// Ce module embarque CPython via pybind11. Le paquet vendorise est celui de
// WINDOWS (son pyconfig.h reclame <io.h>) : sur les autres plateformes on
// compile NkEmbeddedJengaStub.cpp a la place, qui repond « indisponible » et
// laisse l'appelant retomber sur le `jenga` du PATH.
#if defined(NKCODE_EMBED_PYTHON)
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
			bool gProdTools = false; // tools/ de PROD a cote de l'exe (distribution testeur)

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

		bool NkEmbeddedJenga::HasProdTools() {
			return gProdTools;
		}

		namespace {
			NkString gExeDir;
		}

		NkString NkEmbeddedJenga::CompilersDir() {
			return gExeDir + "/tools/compilers";
		}

		bool NkEmbeddedJenga::NeedsCompiler() {
			return gProdTools && !DirExists(CompilersDir() + "/llvm-mingw/bin");
		}

		void NkEmbeddedJenga::Configure(const NkString &exeDir) {
			gExeDir = exeDir;
			// Prod : tools/ a cote de NKCode.exe (pose par le packaging, Phase 5).
			// Dev  : repli sur le PythonEmbed vendorise du repo (exe dans
			// Build/Bin/<cfg>-<os>/NKCode -> racine repo = 4 niveaux au-dessus) et
			// sur NKCODE_JENGA_SRC pour les sources Jenga (repo Jenga local).
			gProdTools = DirExists(exeDir + "/tools/python-embed") &&
						 DirExists(exeDir + "/tools/jenga-src/Jenga");
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
			// Compilateur PAR DEFAUT embarque (tools/compilers/llvm-mingw, pose par
			// scripts/MakeNkCodeDist.py) : prefixe le PATH du PROCESS pour que le
			// ToolchainManager de Jenga (qui detecte clang via PATH, preference
			// `clang-mingw` en tete sur Windows) le trouve — herite par le worker
			// Python ET ses subprocess compilateur. Un clang deja installe par
			// l'utilisateur garde priorite ? NON : le notre est PREFIXE volontairement
			// (comportement reproductible chez tous les testeurs) ; un power-user
			// peut toujours forcer --toolchain ou retirer tools/compilers.
#if defined(_WIN32)
			{
				const NkString binDir = exeDir + "/tools/compilers/llvm-mingw/bin";
				if (DirExists(binDir)) {
					const char *cur = std::getenv("PATH");
					const NkString merged = binDir + ";" + (cur ? cur : "");
					_putenv_s("PATH", merged.CStr());
				}
			}
			// `tools/` contient AUSSI le shim `jenga.cmd` (pose par
			// scripts/MakeNkCodeDist.py) qui appelle le Python embarque. En
			// prefixant le PATH, un `jenga ...` tape dans le TERMINAL INTEGRE — ou
			// lance par une commande qui a besoin d'un vrai terminal, comme
			// `jenga gdb` — fonctionne aussi sans Python installe. Sans ce shim,
			// seules les commandes routees vers l'interpreteur marchaient.
			if (gProdTools) {
				const char *cur = std::getenv("PATH");
				const NkString merged = (exeDir + "/tools") + ";" + (cur ? cur : "");
				_putenv_s("PATH", merged.CStr());
			}
#endif
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

						if (req.kind == "installcompiler") {
							// Distribution legere : telecharge Clang (llvm-mingw) via le
							// module Jenga embarque ; progression via le MEME sink.
							py::module_ cf = py::module_::import("Jenga.Core.CompilerFetch");
							py::object r = cf.attr("InstallDefaultCompiler")(
								py::str(req.target.CStr()), py::arg("sink") = ns);
							exitCode = r.cast<int>();
							if (exitCode == 0) {
#if defined(_WIN32)
								// rend le compilateur visible IMMEDIATEMENT (les builds
								// suivants de la file heriteront de ce PATH).
								const NkString bin = NkString(req.target.CStr()) + "/llvm-mingw/bin";
								const char *cur = std::getenv("PATH");
								const NkString merged = bin + ";" + (cur ? cur : "");
								_putenv_s("PATH", merged.CStr());
#endif
							}
							{
								threading::NkScopedLock<NkMutex> lk(mMutex);
								mExit = exitCode;
								mRunning = false;
								mDone = true;
							}
							continue;
						}
						if (req.kind == "exepath") {
							// Chemin du binaire produit, SANS construire ni lancer. Sert a
							// « Demarrer » : `jenga run` est un processus LONG (et il peut y
							// en avoir plusieurs en parallele), il ne peut donc pas occuper
							// l'interpreteur unique. L'hote construit via l'API embarquee
							// puis lance l'executable NATIVEMENT avec ce chemin — plus
							// aucun `jenga` externe requis.
							py::object pr = embed.attr("ExecutablePath")(
								py::arg("jenga_file") = (req.jengaFile.Empty()
															 ? py::object(py::none())
															 : py::object(py::str(req.jengaFile.CStr()))),
								py::arg("target") = (req.target.Empty() ? py::object(py::none())
																		: py::object(py::str(req.target.CStr()))),
								py::arg("config") = py::str(req.config.Empty() ? "Debug" : req.config.CStr()),
								py::arg("platform") = (req.platform.Empty() ? py::object(py::none())
																			 : py::object(py::str(req.platform.CStr()))),
								py::arg("toolchain") = (req.toolchain.Empty() ? py::object(py::none())
																			   : py::object(py::str(req.toolchain.CStr()))));
							const std::string path = pr.cast<std::string>();
							// Emis comme UNE ligne prefixee : l'hote la reconnait sans
							// nouveau canal de donnees.
							PushLine(NkString("[jenga-exepath] ") + path.c_str());
							exitCode = path.empty() ? 1 : 0;
							threading::NkScopedLock<NkMutex> lk(mMutex);
							mExit = exitCode;
							mRunning = false;
							mDone = true;
							continue;
						}
						if (req.kind == "cli") {
							// N'IMPORTE QUELLE commande Jenga, via le dispatcher de la CLI
							// (Embed.RunCommand -> Jenga.Commands.execute_command). C'est ce
							// qui garantit qu'AUCUNE commande ne retombe sur un `jenga`
							// externe : sur une machine sans Python, elles echoueraient
							// toutes. La sortie passe par le meme sink -> transcript
							// identique a celui d'un sous-processus.
							py::list argv;
							for (usize i = 0; i < req.args.Size(); ++i)
								argv.append(py::str(req.args[i].CStr()));
							py::object r = embed.attr("RunCommand")(argv, py::arg("sink") = ns);
							exitCode = r.attr("exitCode").cast<int>();
							const std::string e2 = r.attr("errorMessage").cast<std::string>();
							if (!e2.empty())
								PushLine(NkString("[jenga-embed] erreur: ") + e2.c_str());
							threading::NkScopedLock<NkMutex> lk(mMutex);
							mExit = exitCode;
							mRunning = false;
							mDone = true;
							continue;
						}
						if (req.kind == "info") {
							// `jenga info` in-process. On REPRODUIT le format des tables
							// du CLI (« Name / Kind » puis « Name / Family / Target OS /
							// Arch / Env ») : NkCodeState::ParseProjects fonctionne alors
							// SANS AUCUNE modification, et le meme code de parsing sert
							// aux deux modes (aucune divergence possible).
							py::object wi = embed.attr("Info")(
								py::arg("jenga_file") = (req.jengaFile.Empty()
															 ? py::object(py::none())
															 : py::object(py::str(req.jengaFile.CStr()))));
							const std::string err = wi.attr("errorMessage").cast<std::string>();
							if (!err.empty()) {
								PushLine(NkString("Error: ") + err.c_str());
								exitCode = 1;
							} else {
								{ // Configurations: A, B
									NkString line("Configurations: ");
									bool first = true;
									for (auto c : wi.attr("configurations")) {
										if (!first)
											line += ", ";
										first = false;
										line += c.cast<std::string>().c_str();
									}
									PushLine(line);
								}
								PushLine(NkString(""));
								PushLine(NkString("Name                Kind"));
								PushLine(NkString("----                ----"));
								for (auto p : wi.attr("projects")) {
									NkString l(p.attr("name").cast<std::string>().c_str());
									l += "  ";
									l += p.attr("kind").cast<std::string>().c_str();
									PushLine(l);
								}
								PushLine(NkString(""));
								PushLine(NkString("Name                Family    Target OS  Arch    Env"));
								PushLine(NkString("----                ------    ---------  ----    ---"));
								for (auto t : wi.attr("toolchains")) {
									NkString l(t.attr("name").cast<std::string>().c_str());
									l += "  ";
									l += t.attr("family").cast<std::string>().c_str();
									l += "  ";
									l += t.attr("targetOs").cast<std::string>().c_str();
									l += "  ";
									l += t.attr("arch").cast<std::string>().c_str();
									const std::string env = t.attr("env").cast<std::string>();
									l += "  ";
									l += env.empty() ? "-" : env.c_str();
									PushLine(l);
								}
								PushLine(NkString(""));
								exitCode = 0;
							}
							threading::NkScopedLock<NkMutex> lk(mMutex);
							mExit = exitCode;
							mRunning = false;
							mDone = true;
							continue;
						}
						const char *fn = (req.kind == "rebuild")  ? "Rebuild"
										 : (req.kind == "clean") ? "Clean"
										 : (req.kind == "test")	 ? "Test"
																 : "Build";
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

#endif // NKCODE_EMBED_PYTHON
