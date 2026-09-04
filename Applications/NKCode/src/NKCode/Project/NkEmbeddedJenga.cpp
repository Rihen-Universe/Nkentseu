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
#include "NKFileSystem/NkFile.h"          // lecture de Jenga/_version.py
#include "NKCode/Project/NkText.h"        // NkFindSub

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

		// Dossier a PREFIXER au PATH pour rendre le compilateur par defaut visible.
		// Doit rester le MIROIR de CompilerFetch.BinDir() : llvm-mingw expose ses
		// binaires dans bin/ sous Windows, l'archive Zig pose `zig` a la racine.
		// Version du Jenga EMBARQUE, lue directement dans Jenga/_version.py.
		// Volontairement SANS interpreteur : une lecture de fichier est utilisable
		// depuis n'importe quel thread et n'occupe pas le worker unique — la
		// detection de version tourne en tache de fond pendant qu'un build peut
		// deja etre en cours.
		NkString NkEmbeddedJenga::EmbeddedVersion() {
			const NkString f = gJengaSrc + "/Jenga/_version.py";
			if (gJengaSrc.Empty() || !NkFile::Exists(f.CStr()))
				return NkString();
			const NkString txt = NkFile::ReadAllText(NkPath(f.CStr()));
			const char *k = NkFindSub(txt.CStr(), "__version__");
			if (!k)
				return NkString();
			// __version__ = "2.1.0"  ->  2.1.0 (guillemets simples ou doubles)
			const char *q = k;
			while (*q && *q != 0x22 && *q != 0x27 && *q != 0x0A)
				++q;
			if (!*q || *q == 0x0A)
				return NkString();
			const char quote = *q++;
			NkString v;
			while (*q && *q != quote)
				v += *q++;
			return v;
		}

		NkString NkEmbeddedJenga::DefaultCompilerBin() {
#if defined(_WIN32)
			return CompilersDir() + "/llvm-mingw/bin";
#else
			return CompilersDir() + "/zig";
#endif
		}

		bool NkEmbeddedJenga::NeedsCompiler() {
			// Le compilateur par defaut DEPEND DE L'HOTE (cf. CompilerFetch) : tester
			// « llvm-mingw » en dur rendait ceci TOUJOURS vrai sous Linux/macOS, ou ce
			// dossier n'existe jamais — NKCode y proposait donc en boucle d'installer
			// un compilateur qui n'aurait de toute facon pas pu servir.
			return gProdTools && !DirExists(DefaultCompilerBin());
		}

		// ── Aides locales a Configure() ────────────────────────────────────────
		namespace {
			// Comparaison de chaines sans dependance : NkText.h fournit NkFindSub,
			// pas d'egalite stricte, et on ne veut pas tirer <cstring> pour ca.
			bool SameStr(const char *a, const char *b) {
				if (!a || !b) return a == b;
				while (*a && *a == *b) { ++a; ++b; }
				return *a == *b;
			}

			// Pose PATH dans l'environnement du PROCESS — celui dont heritent le
			// terminal integre, le worker Python et les sous-processus compilateur.
			void SetProcessPath(const NkString &value) {
#if defined(_WIN32)
				_putenv_s("PATH", value.CStr());
#else
				::setenv("PATH", value.CStr(), 1);
#endif
			}

			// Dossier personnel, sans passer par NkOpenWsState (Shell/) : Project/
			// ne doit pas dependre de la coquille, et Configure() tourne avant elle.
			NkString HomeDir() {
#if defined(_WIN32)
				const char *h = std::getenv("USERPROFILE");
#else
				const char *h = std::getenv("HOME");
#endif
				return NkString(h ? h : "");
			}

			// La preference compilateur : NKCODE_COMPILER (env) prime, sinon la
			// ligne `compiler=` de ~/.nkcode/settings.cfg. Vide si aucune des deux.
			// Le fichier est du key=value une ligne par cle, ecrit par
			// NkSettingsState::Save() ; on ne lit QUE cette cle, sans parseur.
			NkString ReadCompilerPreference() {
				if (const char *env = std::getenv("NKCODE_COMPILER"); env && *env)
					return NkString(env);
				const NkString home = HomeDir();
				if (home.Empty())
					return NkString();
				const NkString cfg = home + "/.nkcode/settings.cfg";
				if (!NkFile::Exists(cfg.CStr()))
					return NkString();
				const NkString txt = NkFile::ReadAllText(cfg.CStr());
				const char *p = txt.CStr();
				static const char kKey[] = "compiler=";
				while (*p) {
					// debut de ligne : la cle doit y etre EXACTEMENT, pas en sous-chaine
					const char *k = kKey;
					const char *q = p;
					while (*k && *q == *k) { ++q; ++k; }
					if (!*k) {
						NkString v;
						while (*q && *q != '\r' && *q != '\n') v += *q++;
						return v;
					}
					while (*p && *p != '\n') ++p;
					if (*p) ++p;
				}
				return NkString();
			}
		} // namespace

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
				// En DEV, le runtime vendorise differe par plateforme : le paquet
				// embarquable de python.org sous Windows (runtime/ a plat), et une
				// distribution relogeable python-build-standalone sous Linux
				// (linux/ avec bin/, lib/, include/). Les deux atterrissent sur
				// tools/python-embed une fois empaquetes.
#if defined(_WIN32)
				const NkString dev = devRoot + "/Externals/Libs/PythonEmbed/runtime";
#elif defined(__APPLE__)
				// macOS : python-build-standalone pose sous macos/ (meme
				// disposition UNIX que linux/, cf. NKCode.jenga).
				const NkString dev = devRoot + "/Externals/Libs/PythonEmbed/macos";
#else
				const NkString dev = devRoot + "/Externals/Libs/PythonEmbed/linux";
#endif
				if (DirExists(dev))
					gPyHome = dev;
			}
			gJengaSrc = exeDir + "/tools/jenga-src";
			if (!DirExists(gJengaSrc + "/Jenga")) {
				const char *env = std::getenv("NKCODE_JENGA_SRC");
				if (env && *env && DirExists(NkString(env) + "/Jenga"))
					gJengaSrc = env;
			}
			// ── Compilateur : L'UTILISATEUR CHOISIT ──────────────────────────────
			//
			// Avant : le compilateur embarque (tools/compilers/llvm-mingw sous
			// Windows, tools/compilers/zig ailleurs) etait PREFIXE au PATH sans
			// appel, « pour un comportement reproductible chez tous les testeurs ».
			// Consequence : quelqu'un qui avait deja clang, gcc ou MSVC sur sa
			// machine se retrouvait a compiler avec le notre sans l'avoir demande,
			// et sans moyen simple de reprendre la main — le commentaire renvoyait a
			// `--toolchain` ou a la suppression de tools/compilers. Decision du
			// 2026-09-04 : ce n'est pas a l'outil de decider avec quoi on compile.
			//
			// UNE preference, trois valeurs, lue ici avant tout le reste :
			//   ""  ou "embarque"  -> le compilateur de NKCode est PREFIXE (defaut :
			//                         quelqu'un qui n'a rien obtient quelque chose)
			//   "systeme"          -> il est SUFFIXE : le compilateur deja installe
			//                         garde la main, le notre ne sert qu'en repli
			//   un chemin          -> ce dossier bin/ est prefixe a la place du notre
			//
			// Source, par priorite : la variable NKCODE_COMPILER (scripts, tests),
			// puis la ligne `compiler=` de ~/.nkcode/settings.cfg — celle que
			// l'ecran Parametres ecrit. Configure() tourne avant que NkSettings
			// existe, d'ou la lecture directe du fichier ici.
			//
			// Herite par le terminal integre (CreateProcessW/forkpty sans
			// environnement explicite), par le worker Python et par les
			// sous-processus compilateur de Jenga : un seul reglage, partout.
#if defined(_WIN32)
			const char kPathSep = ';';
#else
			const char kPathSep = ':';
#endif
			{
				const NkString pref  = ReadCompilerPreference();
				const NkString ours  = DefaultCompilerBin();
				NkString dirToAdd;
				bool prefix = true;
				if (SameStr(pref.CStr(), "systeme")) {
					dirToAdd = ours;  prefix = false;
				} else if (pref.Empty() || SameStr(pref.CStr(), "embarque")) {
					dirToAdd = ours;  prefix = true;
				} else {
					dirToAdd = pref;  prefix = true;   // dossier bin/ choisi par l'utilisateur
				}
				if (DirExists(dirToAdd)) {
					const char *cur = std::getenv("PATH");
					const NkString c = cur ? cur : "";
					const NkString merged = prefix ? (dirToAdd + kPathSep + c)
					                               : (c + kPathSep + dirToAdd);
					SetProcessPath(merged);
				}
			}
			// `tools/` contient le shim `jenga` (jenga.cmd sous Windows, script sh
			// ailleurs, poses par scripts/MakeNkCodeDist.py) qui appelle le Python
			// embarque. En prefixant le PATH, un `jenga ...` tape dans le TERMINAL
			// INTEGRE — ou lance par une commande qui a besoin d'un vrai terminal,
			// comme `jenga gdb` — fonctionne aussi sans Python installe. Ce shim,
			// lui, reste toujours prefixe : il n'entre en concurrence avec rien.
			if (gProdTools) {
				const char *cur = std::getenv("PATH");
				const NkString merged = (exeDir + "/tools") + kPathSep + (cur ? cur : "");
				SetProcessPath(merged);
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
			const std::wstring src = WidenUtf8(gJengaSrc);
#if defined(_WIN32)
			// Windows : « embeddable package » officiel de python.org. La
			// bibliotheque standard est un ZIP a cote du DLL, et les modules
			// d'extension (.pyd) vivent dans DLLs/.
			const std::wstring zip = WidenUtf8(gPyHome + "/python312.zip");
			const std::wstring dlls = WidenUtf8(gPyHome + "/DLLs");
			PyWideStringList_Append(&config.module_search_paths, zip.c_str());
			PyWideStringList_Append(&config.module_search_paths, home.c_str());
			PyWideStringList_Append(&config.module_search_paths, dlls.c_str());
#else
			// Linux : distribution relogeable python-build-standalone, dont la
			// disposition suit celle d'une installation UNIX classique. La
			// bibliotheque standard est une ARBORESCENCE (pas un zip) sous
			// lib/pythonX.Y, et les modules d'extension (.so) sous lib-dynload.
			// Reprendre les chemins Windows ici donnerait un interpreteur sans
			// aucun module, qui echouerait des le premier `import`.
			const std::wstring stdlib = WidenUtf8(gPyHome + "/lib/python" NKCODE_PYEMBED_VER);
			const std::wstring dynload = WidenUtf8(gPyHome + "/lib/python" NKCODE_PYEMBED_VER "/lib-dynload");
			PyWideStringList_Append(&config.module_search_paths, stdlib.c_str());
			PyWideStringList_Append(&config.module_search_paths, dynload.c_str());
			PyWideStringList_Append(&config.module_search_paths, home.c_str());
#endif
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
						// Le texte des avertissements ne remontait pas : seul un booleen
						// passait par OnFileDone, de quoi allumer un voyant mais pas de quoi
						// dire OU ni QUOI. Cette sortie-ci est BRUTE — c'est elle qui porte
						// le chemin complet et les numeros de ligne, la ou l'affichage
						// console les tronque pour tenir dans son cadre.
						ns.attr("OnCompileWarning") =
							py::cpp_function([this](std::string proj, std::string file, std::string msg) {
								NkJengaProgressEvent e;
								e.kind = NkJengaProgressEvent::COMPILE_WARNING;
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
								// Rend le compilateur visible IMMEDIATEMENT (les builds
								// suivants de la file heriteront de ce PATH). Vaut pour
								// TOUTES les plateformes : sans cela, sous Linux/macOS,
								// le Zig fraichement installe restait introuvable jusqu'au
								// redemarrage de NKCode.
								const NkString bin = DefaultCompilerBin();
								const char *cur = std::getenv("PATH");
#if defined(_WIN32)
								const NkString merged = bin + ";" + (cur ? cur : "");
								_putenv_s("PATH", merged.CStr());
#else
								const NkString merged = bin + ":" + (cur ? cur : "");
								setenv("PATH", merged.CStr(), 1);
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
																			   : py::object(py::str(req.toolchain.CStr()))),
								py::arg("withKind") = py::bool_(true));
							// withKind=True -> « <Kind>|<chemin> » (ex. « ConsoleApp|D:/.../a.exe »).
							// Le KIND decide OU lancer : une ConsoleApp merite un vrai
							// terminal (stdin, ANSI, code de sortie), pas une WindowedApp.
							std::string path = pr.cast<std::string>();
							std::string kind;
							const std::string::size_type bar = path.find('|');
							if (bar != std::string::npos) {
								kind = path.substr(0, bar);
								path = path.substr(bar + 1);
							}
							// Emis comme des lignes prefixees : l'hote les reconnait sans
							// nouveau canal de donnees.
							PushLine(NkString("[jenga-exekind] ") + kind.c_str());
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
								// Projet de DEMARRAGE : MEME ligne que le CLI (`jenga info`), pour
								// que l'hote n'ait qu'UN seul format a analyser. Sans elle,
								// « Demarrer » sur « tous les projets » aurait respecte le
								// startproject avec Jenga externe mais pas en embarque — deux
								// comportements pour une meme action.
								{
									const std::string sp = wi.attr("startProject").cast<std::string>();
									if (!sp.empty())
										PushLine(NkString("Start project: ") + sp.c_str());
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
