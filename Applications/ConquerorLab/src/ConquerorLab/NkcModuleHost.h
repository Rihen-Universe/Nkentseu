#pragma once
// =============================================================================
// NkcModuleHost — catalogue des moteurs de regles et des IA disponibles.
//
// CE QUE VOIT LE STAGIAIRE
// ------------------------
//   1. il ecrit  Build/ConquerorLab/rules/mes_regles.cpp
//   2. il sauvegarde
//   3. l'atelier detecte, COMPILE, charge, et son module apparait dans le menu
//   4. il rejoue une partie sans avoir quitte l'application
//
// Il ne voit jamais de .dll : le binaire est produit a cote du .cpp et gere ici.
// En cas d'erreur de compilation, le log est conserve dans l'entree du module
// et affiche par le panneau « Modules ».
//
// PLATEFORMES
//   PC            : modules compiles et charges dynamiquement, rechargement a chaud.
//   Android / Web : pas de compilateur sur l'appareil -> seuls les modules
//                   compiles DANS l'application sont disponibles. L'atelier
//                   reste jouable et mesurable ; seul le hot-reload disparait.
// =============================================================================

// EN PREMIER : desamorce les macros de <windows.h> (cf. NkcWinClean.h). Ce
// fichier apporte AUSSI <windows.h> sur Windows, dont LoadLibraryA/GetProcAddress
// utilises plus bas.
#include "ConquerorLab/NkcWinClean.h"

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorAIABI.h"
#include "ConquerorLab/NkcModuleCompiler.h"
#include "ConquerorLab/NkcModuleLog.h"

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFileSystem.h"
#include "NKFileSystem/NkPath.h"
#include "NKLogger/NkLog.h"

#include <cstring>

#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#	define NKC_DYNAMIC_MODULES 0
#else
#	define NKC_DYNAMIC_MODULES 1
#	if !defined(_WIN32)
#		include <dlfcn.h>
#	endif
// Sur Windows, <windows.h> est deja venu (proprement) par NkcWinClean.h.
#endif

// Fabriques compilees dans l'application — disponibles sur TOUTES les plateformes.
void NkcRulesV2GetFactory(nkentseu::conqueror::NkcRulesFactory *out);
void NkcRulesV2SetAllocator(nkentseu::conqueror::NkcAllocFn a, nkentseu::conqueror::NkcFreeFn f);
void NkcAIRefGetFactory(nkentseu::conqueror::NkcAIFactory *out);
void NkcAIRefSetAllocator(nkentseu::conqueror::NkcAllocFn a, nkentseu::conqueror::NkcFreeFn f);

namespace nkentseu {
	namespace conqueror {

		/// Etat d'un module dans le catalogue.
		enum class NkcModuleStatus : uint8 {
			Builtin		 = 0,  ///< compile dans l'application
			Ready		 = 1,  ///< compile et charge
			CompileError = 2,  ///< le .cpp n'a pas compile
			LoadError	 = 3   ///< compile mais refuse au chargement (ABI, symbole)
		};

		struct NkcModuleEntry {
				NkString		label;		 ///< ce que voit l'utilisateur
				NkString		sourcePath;	 ///< le .cpp du stagiaire (vide si interne)
				NkString		binaryPath;	 ///< le binaire produit (vide si interne)
				NkString		log;		 ///< sortie compilateur / message d'erreur
				void		   *handle	= nullptr;
				nk_int64		srcTime = 0;
				NkcModuleStatus status	= NkcModuleStatus::Builtin;
				/// Charge, mais compile contre une ABI mineure anterieure : il tourne,
				/// sans les dernieres entrees. Le panneau Modules le signale.
				bool			stale	= false;

				bool Usable() const noexcept {
					return status == NkcModuleStatus::Builtin || status == NkcModuleStatus::Ready;
				}
		};

		struct NkcRulesEntry : NkcModuleEntry {
				NkcRulesFactory factory{};
		};

		struct NkcAIEntry : NkcModuleEntry {
				NkcAIFactory factory{};
		};

		// ---------------------------------------------------------------------
		class NkcModuleHost {
			public:
				/// `log` peut etre nullptr : l'atelier fonctionne sans journal de
				/// modules, les traces repartent alors sur stderr cote module.
				void Init(const NkcLayout &layout, NkcModuleLog *log = nullptr) noexcept {
					mLayout	  = &layout;
					mRulesDir = layout.WorkDir("rules");
					mAiDir	  = layout.WorkDir("ai");
					mLog	  = log;
					NkDirectory::CreateRecursive(mRulesDir.CStr());
					NkDirectory::CreateRecursive(mAiDir.CStr());
					mCompiler.Init(layout);
					RegisterBuiltins();
					Refresh();
				}

				void Shutdown() noexcept {
					for (usize i = 0; i < mRules.Size(); ++i) Unload(mRules[i].handle);
					for (usize i = 0; i < mAis.Size(); ++i) Unload(mAis[i].handle);
					mRules.Clear();
					mAis.Clear();
				}

				const NkVector<NkcRulesEntry> &Rules() const noexcept { return mRules; }
				const NkVector<NkcAIEntry>	  &Ais() const noexcept { return mAis; }

				bool CanCompile() const noexcept { return mCompiler.Available(); }
				const NkString &CompilerPath() const noexcept { return mCompiler.CompilerPath(); }

				/// Dossiers surveilles. Le panneau « Modules » les affiche : le
				/// stagiaire doit savoir OU deposer son fichier sans le demander.
				const NkString &RulesDir() const noexcept { return mRulesDir; }
				const NkString &AiDir() const noexcept { return mAiDir; }

				/// Le chargement dynamique est-il possible sur cette plateforme ?
				/// Faux sur Android et Web : pas de compilateur sur l'appareil.
				static bool SupportsDynamic() noexcept { return NKC_DYNAMIC_MODULES != 0; }

				/// Scanne les dossiers, compile ce qui doit l'etre, (re)charge.
				/// Renvoie le nombre de modules qui ont change d'etat.
				uint32 Refresh() noexcept {
					uint32 changed = 0;
#if NKC_DYNAMIC_MODULES
					changed += RefreshRules();
					changed += RefreshAis();
#endif
					return changed;
				}

				/// Force la recompilation d'un module, quel que soit son horodatage.
				void ForceRebuild(const NkString &sourcePath) noexcept {
#if NKC_DYNAMIC_MODULES
					const NkString bin = NkcModuleCompiler::ReplaceExtension(
						sourcePath, NkcModuleCompiler::BinaryExt());
					NkFile::Delete(bin.CStr());
					Refresh();
#else
					(void)sourcePath;
#endif
				}

			private:
				// ---- fabriques internes ------------------------------------
				void RegisterBuiltins() noexcept {
					{
						NkcRulesEntry e;
						e.label	 = "ConquerorRulesV2 (interne)";
						e.status = NkcModuleStatus::Builtin;
						NkcRulesV2SetAllocator(nullptr, nullptr);
						NkcRulesV2GetFactory(&e.factory);
						if (e.factory.info.abiVersion != kRulesAbiVersion) {
							e.status = NkcModuleStatus::LoadError;
							e.log	 = "Version d'ABI incompatible (module interne).";
							logger.Error("[lab] regles internes : ABI incompatible");
						}
						mRules.PushBack(e);
					}
					{
						NkcAIEntry e;
						e.label	 = "AIRef (interne)";
						e.status = NkcModuleStatus::Builtin;
						NkcAIRefSetAllocator(nullptr, nullptr);
						NkcAIRefGetFactory(&e.factory);
						if (e.factory.info.abiVersion != kAiAbiVersion) {
							e.status = NkcModuleStatus::LoadError;
							e.log	 = "Version d'ABI incompatible (module interne).";
							logger.Error("[lab] IA interne : ABI incompatible");
						}
						mAis.PushBack(e);
					}
				}

#if NKC_DYNAMIC_MODULES
				static void *OpenLib(const char *path) noexcept {
#	if defined(_WIN32)
					return reinterpret_cast<void *>(::LoadLibraryA(path));
#	else
					return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#	endif
				}
				static void *SymOf(void *h, const char *n) noexcept {
					if (!h) return nullptr;
#	if defined(_WIN32)
					// FARPROC -> void* : conversion explicite, la seule que Windows
					// autorise pour un pointeur de fonction recupere par nom.
					return reinterpret_cast<void *>(::GetProcAddress(static_cast<HMODULE>(h), n));
#	else
					return dlsym(h, n);
#	endif
				}
				static void Unload(void *h) noexcept {
					if (!h) return;
#	if defined(_WIN32)
					::FreeLibrary(static_cast<HMODULE>(h));
#	else
					dlclose(h);
#	endif
				}

				/// Copie fantome : sans elle, Windows verrouille le binaire et le
				/// stagiaire ne peut pas recompiler pendant que l'atelier tourne.
				static NkString ShadowCopy(const NkString &src) noexcept {
#	if defined(_WIN32)
					NkString dst = src;
					dst += ".hot";
					NkFile::Delete(dst.CStr());
					if (!NkFile::Copy(src.CStr(), dst.CStr(), true)) return src;
					return dst;
#	else
					return src;
#	endif
				}

				/// Compile si necessaire puis charge. Remplit `e` (status + log).
				template <typename TEntry, typename TFactory>
				bool BuildAndLoad(TEntry &e, const char *symFactory, const char *symAlloc,
								  const char *symLogger, uint32 expectedAbi,
								  uint32 expectedMinor) noexcept {
					e.binaryPath = NkcModuleCompiler::ReplaceExtension(
						e.sourcePath, NkcModuleCompiler::BinaryExt());

					if (NkcModuleCompiler::NeedsRebuild(e.sourcePath, e.binaryPath)) {
						const NkcCompileResult cr = mCompiler.Compile(e.sourcePath);
						e.log = cr.log;
						if (!cr.success) {
							e.status = NkcModuleStatus::CompileError;
							logger.Errorf("[lab] compilation echouee : %s", e.sourcePath.CStr());
							return false;
						}
					}

					Unload(e.handle);
					e.handle = OpenLib(ShadowCopy(e.binaryPath).CStr());
					if (!e.handle) {
						e.status = NkcModuleStatus::LoadError;
						e.log	 = "Chargement du binaire impossible.";
						return false;
					}

					using GetFacFn = void (*)(TFactory *);
					using SetAllFn = void (*)(NkcAllocFn, NkcFreeFn);
					auto getFac = reinterpret_cast<GetFacFn>(SymOf(e.handle, symFactory));
					auto setAll = reinterpret_cast<SetAllFn>(SymOf(e.handle, symAlloc));
					if (!getFac) {
						e.status = NkcModuleStatus::LoadError;
						e.log	 = "Symbole introuvable : ";
						e.log += symFactory;
						e.log += " — as-tu bien termine ton fichier par la macro d'export ?";
						Unload(e.handle);
						e.handle = nullptr;
						return false;
					}
					if (setAll) setAll(nullptr, nullptr);

					std::memset(&e.factory, 0, sizeof(e.factory));
					getFac(&e.factory);

					// ---- controle de version -----------------------------
					// La MAJEURE seule est eliminatoire. Une MINEURE en retard
					// veut dire « ce module ne connait pas les dernieres entrees
					// de la vtable » — or elles sont toutes optionnelles et
					// gardees par un test de nullite : le module tourne.
					//
					// Verifie le 2026-08-07 : un module compile contre l'ABI
					// precedente joue une partie complete, zero coup illegal.
					// C'est le CONTROLE qui l'ecartait, pas le code.
					if (e.factory.info.abiVersion != expectedAbi) {
						e.status = NkcModuleStatus::LoadError;
						char tmp[220];
						std::snprintf(tmp, sizeof(tmp),
									  "ABI majeure %u, attendue %u — incompatible.\n"
									  "Le contrat a change de forme : recompile ton module "
									  "avec les en-tetes a jour.",
									  e.factory.info.abiVersion, expectedAbi);
						e.log = tmp;
						Unload(e.handle);
						e.handle = nullptr;
						return false;
					}

					// Mineure en retard : on ACCEPTE, et on le dit.
					if (e.factory.abiMinor < expectedMinor) {
						char tmp[300];
						std::snprintf(tmp, sizeof(tmp),
									  "Compile et charge.\n"
									  "Note : module en ABI %u.%u, atelier en %u.%u. Il fonctionne — "
									  "les fonctions ajoutees depuis ne sont simplement pas "
									  "disponibles. Recompile quand tu veux en profiter.",
									  e.factory.info.abiVersion, e.factory.abiMinor,
									  expectedAbi, expectedMinor);
						e.log = tmp;
						e.stale = true;
					}

					e.status = NkcModuleStatus::Ready;
					e.label	 = e.factory.info.name;
					e.label += " (";
					e.label += e.factory.info.version;
					e.label += ")";
					e.log = "Compile et charge.";

					// Brancher le journal du module sur celui de l'atelier. APRES
					// getFac : le nom affiche dans le panneau « Sortie » est celui
					// que le module declare, pas le nom de fichier.
					//
					// Le symbole est OPTIONNEL. Un module qui ne journalise pas n'a
					// pas a s'encombrer d'un export de plus — son absence n'est donc
					// ni une erreur ni un avertissement.
					InstallLogger(e.handle, symLogger, e.factory.info.name);

					logger.Infof("[lab] module pret : %s", e.label.CStr());
					return true;
				}

				/// Pose le puits de journal dans un module fraichement charge.
				void InstallLogger(void *handle, const char *symLogger,
								   const char *name) noexcept {
					if (!mLog || !symLogger) return;
					using SetLogFn = void (*)(NkcLogFn, void *, const char *);
					auto setLog = reinterpret_cast<SetLogFn>(SymOf(handle, symLogger));
					if (setLog) setLog(&NkcModuleLog::Sink, mLog, name);
				}

				template <typename TEntry>
				static int32 IndexOfSource(const NkVector<TEntry> &v, const NkString &src) noexcept {
					for (usize i = 0; i < v.Size(); ++i)
						if (v[i].sourcePath == src) return static_cast<int32>(i);
					return -1;
				}

				/// ⚠️ `NkDirectory::GetFiles` rend des chemins COMPLETS
				/// (NkDirectory.cpp:355), pas des noms de fichiers. Les recoller
				/// derriere `mRulesDir` fabriquait
				/// `<rules>/D:/.../rules/mes_regles.cpp` : un chemin impossible, donc
				/// AUCUN module de stagiaire n'etait jamais compile ni charge, et le
				/// seul indice etait un « compilation echouee » suivi d'un chemin
				/// double que personne ne lit en entier. Mesure du 2026-08-15.
				/// Meme defaut, meme jour, dans NkcBoardLibrary.
				uint32 RefreshRules() noexcept {
					uint32 changed = 0;
					NkVector<NkString> files = NkDirectory::GetFiles(mRulesDir.CStr(), "*.cpp");
					for (usize i = 0; i < files.Size(); ++i) {
						const NkString full = files[i];	  // deja complet
						const NkString name = NkPath(full).GetFileName();
						const nk_int64 t = NkFileSystem::GetLastWriteTime(full.CStr());
						const int32 idx	 = IndexOfSource(mRules, full);
						if (idx >= 0) {
							if (mRules[static_cast<usize>(idx)].srcTime == t) continue;
							mRules[static_cast<usize>(idx)].srcTime = t;
							BuildAndLoad<NkcRulesEntry, NkcRulesFactory>(
								mRules[static_cast<usize>(idx)],
								NKC_RULES_SYM_GET_FACTORY, NKC_RULES_SYM_SET_ALLOC,
								NKC_RULES_SYM_SET_LOGGER, kRulesAbiMajor, kRulesAbiMinor);
							++changed;
						} else {
							NkcRulesEntry e;
							e.sourcePath = full;
							e.label		 = name;   // « mes_regles.cpp », pas le chemin entier
							e.srcTime	 = t;
							BuildAndLoad<NkcRulesEntry, NkcRulesFactory>(
								e, NKC_RULES_SYM_GET_FACTORY, NKC_RULES_SYM_SET_ALLOC,
								NKC_RULES_SYM_SET_LOGGER, kRulesAbiMajor, kRulesAbiMinor);
							mRules.PushBack(e);
							++changed;
						}
					}
					return changed;
				}

				/// Meme correctif que `RefreshRules` : chemins deja complets.
				uint32 RefreshAis() noexcept {
					uint32 changed = 0;
					NkVector<NkString> files = NkDirectory::GetFiles(mAiDir.CStr(), "*.cpp");
					for (usize i = 0; i < files.Size(); ++i) {
						const NkString full = files[i];	  // deja complet
						const NkString name = NkPath(full).GetFileName();
						const nk_int64 t = NkFileSystem::GetLastWriteTime(full.CStr());
						const int32 idx	 = IndexOfSource(mAis, full);
						if (idx >= 0) {
							if (mAis[static_cast<usize>(idx)].srcTime == t) continue;
							mAis[static_cast<usize>(idx)].srcTime = t;
							BuildAndLoad<NkcAIEntry, NkcAIFactory>(
								mAis[static_cast<usize>(idx)],
								NKC_AI_SYM_GET_FACTORY, NKC_AI_SYM_SET_ALLOC,
								NKC_AI_SYM_SET_LOGGER, kAiAbiMajor, kAiAbiMinor);
							++changed;
						} else {
							NkcAIEntry e;
							e.sourcePath = full;
							e.label		 = name;   // « mon_ia.cpp », pas le chemin entier
							e.srcTime	 = t;
							BuildAndLoad<NkcAIEntry, NkcAIFactory>(
								e, NKC_AI_SYM_GET_FACTORY, NKC_AI_SYM_SET_ALLOC,
								NKC_AI_SYM_SET_LOGGER, kAiAbiMajor, kAiAbiMinor);
							mAis.PushBack(e);
							++changed;
						}
					}
					return changed;
				}
#else
				static void Unload(void *) noexcept {}
#endif

			private:
				NkString				mRulesDir;
				NkString				mAiDir;
				NkcModuleCompiler		mCompiler;
				NkcModuleLog		   *mLog = nullptr;
				const NkcLayout		   *mLayout = nullptr;
				NkVector<NkcRulesEntry> mRules;
				NkVector<NkcAIEntry>	mAis;
		};

	} // namespace conqueror
} // namespace nkentseu
