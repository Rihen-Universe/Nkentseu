#pragma once
// =============================================================================
// NkcModuleCompiler — compile un .cpp de stagiaire en module chargeable.
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// Le stagiaire ne doit jamais manipuler de binaire. Il ecrit du C++, le
// sauvegarde dans un dossier, et l'atelier s'occupe du reste :
//
//     rules/mes_regles.cpp   --[ce module]-->   rules/mes_regles.dll   --> charge
//
// En cas d'erreur, la sortie du compilateur est capturee et affichee dans le
// panneau « Modules » : pas de terminal, pas de chaine de build a apprendre.
//
// PLATEFORMES
// -----------
// Compilation a l'execution = PC uniquement (Windows, Linux, macOS). Sur
// Android et Web il n'y a pas de compilateur sur l'appareil : les modules de
// reference y sont compiles DANS l'application (cf. NkcModuleHost). C'est une
// limite reelle et assumee, pas un oubli.
// =============================================================================

// EN PREMIER : desamorce les macros de <windows.h> qui reecriraient les
// declarations de NKFileSystem (cf. NkcWinClean.h).
#include "ConquerorLab/NkcWinClean.h"

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFileSystem.h"
#include "NKLogger/NkLog.h"

#include <cstdlib>
#include <cstdio>

#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#	define NKC_CAN_COMPILE 0
#else
#	define NKC_CAN_COMPILE 1
#endif

namespace nkentseu {
	namespace conqueror {

		/// Resultat d'une tentative de compilation.
		struct NkcCompileResult {
				bool	 success = false;
				NkString outputPath;   ///< binaire produit
				NkString log;		   ///< sortie complete du compilateur
		};

		class NkcModuleCompiler {
			public:
				/// `repoRoot` = racine du depot Nkentseu (pour les -I).
				void Init(const NkString &repoRoot) noexcept {
					mRoot = repoRoot;
					DetectCompiler();
				}

				bool Available() const noexcept {
#if NKC_CAN_COMPILE
					return !mCxx.Empty();
#else
					return false;
#endif
				}

				const NkString &CompilerPath() const noexcept { return mCxx; }

				/// Extension du binaire produit sur cette plateforme.
				static const char *BinaryExt() noexcept {
#if defined(_WIN32)
					return ".dll";
#elif defined(__APPLE__)
					return ".dylib";
#else
					return ".so";
#endif
				}

				/// Compile `srcPath` (.cpp) en module partage a cote de lui.
				NkcCompileResult Compile(const NkString &srcPath) noexcept {
					NkcCompileResult res;
					res.outputPath = ReplaceExtension(srcPath, BinaryExt());

#if !NKC_CAN_COMPILE
					res.log = "Compilation a l'execution indisponible sur cette "
							  "plateforme. Les modules doivent etre compiles dans "
							  "l'application.";
					return res;
#else
					if (mCxx.Empty()) {
						res.log = "Aucun compilateur C++ trouve. Posez la variable "
								  "d'environnement NK_CXX sur le chemin de clang++.";
						return res;
					}
					if (!NkFile::Exists(srcPath.CStr())) {
						res.log = "Source introuvable : ";
						res.log += srcPath;
						return res;
					}

					// Fichier temporaire pour capturer la sortie du compilateur.
					NkString logPath = res.outputPath;
					logPath += ".log";
					NkFile::Delete(logPath.CStr());
					NkFile::Delete(res.outputPath.CStr());

					const NkString cmd = BuildCommand(srcPath, res.outputPath, logPath);
					logger.Infof("[lab] compilation : %s", srcPath.CStr());

					const int rc = std::system(cmd.CStr());

					res.log = ReadAll(logPath);
					NkFile::Delete(logPath.CStr());

					res.success = (rc == 0) && NkFile::Exists(res.outputPath.CStr());
					if (!res.success && res.log.Empty()) {
						char tmp[128];
						std::snprintf(tmp, sizeof(tmp),
									  "Echec de compilation (code %d), sans message.", rc);
						res.log = tmp;
					}
					return res;
#endif
				}

				/// Faut-il (re)compiler ? Vrai si le binaire manque ou si la source
				/// est plus recente que lui.
				static bool NeedsRebuild(const NkString &srcPath, const NkString &binPath) noexcept {
					if (!NkFile::Exists(binPath.CStr())) return true;
					const nk_int64 ts = NkFileSystem::GetLastWriteTime(srcPath.CStr());
					const nk_int64 tb = NkFileSystem::GetLastWriteTime(binPath.CStr());
					return ts > tb;
				}

				static NkString ReplaceExtension(const NkString &path, const char *ext) noexcept {
					const usize dot = path.RFind('.');
					NkString out = (dot == NkString::npos) ? path : path.SubStr(0, dot);
					out += ext;
					return out;
				}

			private:
				void DetectCompiler() noexcept {
#if NKC_CAN_COMPILE
					if (const char *env = std::getenv("NK_CXX")) {
						if (env[0] && NkFile::Exists(env)) { mCxx = env; return; }
					}
#	if defined(_WIN32)
					const char *candidates[] = {
						"C:/msys64/ucrt64/bin/clang++.exe",
						"C:/msys64/mingw64/bin/clang++.exe",
						"C:/msys64/ucrt64/bin/g++.exe",
					};
					for (const char *c : candidates) {
						if (NkFile::Exists(c)) { mCxx = c; return; }
					}
					mCxx = "clang++";   // dernier recours : le PATH
#	else
					mCxx = "clang++";
#	endif
#endif
				}

				NkString BuildCommand(const NkString &src, const NkString &out,
									  const NkString &logPath) const noexcept {
					// Les -I du depot : c'est parce que l'atelier les fournit que
					// les modules peuvent utiliser les types Nkentseu.
					NkString inner;
					inner += "\""; inner += mCxx; inner += "\"";
					inner += " -shared -std=c++17 -O2 -fPIC";
#if defined(_WIN32)
					inner += " -static";	// pas de dependance a libstdc++.dll
#endif
					inner += " \"-I"; inner += mRoot; inner += "/Applications/ConquerorLab/include\"";
					inner += " \"-I"; inner += mRoot; inner += "/Kernel/Foundation/NKCore/src\"";
					inner += " \"-I"; inner += mRoot; inner += "/Kernel/Foundation/NKPlatform/src\"";
					inner += " -o \""; inner += out; inner += "\"";
					inner += " \"";	   inner += src; inner += "\"";
					inner += " > \"";  inner += logPath; inner += "\" 2>&1";

#if defined(_WIN32)
					// cmd retire le premier guillemet d'une commande citee : /S /C "..."
					NkString cmd = "cmd /S /C \"";
					cmd += inner;
					cmd += "\"";
					return cmd;
#else
					return inner;
#endif
				}

				static NkString ReadAll(const NkString &path) noexcept {
					NkString out;
					if (!NkFile::Exists(path.CStr())) return out;
					std::FILE *f = std::fopen(path.CStr(), "rb");
					if (!f) return out;
					char buf[1024];
					usize n = 0;
					while ((n = std::fread(buf, 1, sizeof(buf) - 1, f)) > 0) {
						buf[n] = '\0';
						out += buf;
						if (out.Size() > 32768) break;	// borne l'affichage
					}
					std::fclose(f);
					return out;
				}

			private:
				NkString mRoot;
				NkString mCxx;
		};

	} // namespace conqueror
} // namespace nkentseu
