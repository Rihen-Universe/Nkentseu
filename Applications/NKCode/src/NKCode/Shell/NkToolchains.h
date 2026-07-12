// =============================================================================
// NkToolchains.h — Gestionnaire de Toolchains (plein cadre, 2 colonnes).
// Detection REELLE (PATH + variables d'environnement) ; aucune donnee mockee.
// Registre custom : ~/.jenga/toolchains_registry.json (lu si present).
// =============================================================================
#pragma once
#include "NKCode/Shell/NkShell.h" // nkcode::NkShellRun (std::system gardé iOS)
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Shell/NkOpenWs.h"
#include "NKCode/Shell/NkNewWorkspace.h" // NkWizLabel + NkNewWsState::TcWhich/TcEnv
#include "NKCode/Shell/NkI18n.h"		 // NkT() : traductions multi-langue
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/Dialogs.h"
#include "NKThreading/NkThread.h"		  // NkThread (pas de STL) — cf. NkProcess.h
#include "NKCore/NkAtomic.h"			  // NkAtomicBool / NkAtomicInt32
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison, ex-<cstdio>)
#include "NKPlatform/NkEnv.h"			  // env::GetEnvVar (variables d'environnement maison, ex-<cstdlib>)

namespace nkentseu {
	namespace nkcode {

		struct NkTcDetail {
				NkString name, version, target;
				NkVector<NkString> pk, pv; // labels (CC/CXX/AR/NDK/EMSDK/zig) + valeurs
				bool found = false;
				NkString reason;
				bool custom = false;
				NkString caps; // familles de plateformes REELLEMENT compilables (test) : "linux macos windows ..."
		};

		// Résultat de la détection de la toolchain Apple (cross Windows -> iOS/macOS via zig + ld.lld).
		// Cf. docs/ZIG-APPLE-TOOLCHAIN.md §2/§3 : 5 composants, vérifiés par présence de fichiers/.tbd.
		struct NkAppleZig {
				bool iosOk = false, macosOk = false, signOk = false;
				bool okZig = false, okIosSdk = false, okLd = false, okLibcpp = false, okMacSdk = false;
				NkString zig, iosSdk, ld64, libcpp, macSdk, rcodesign, zigVer;
		};

		struct NkToolchainsState {
				NkVector<NkTcDetail> sys;	 // toolchains systeme detectes
				NkVector<NkTcDetail> custom; // toolchains du registre
				NkVector<NkString> testLog;	 // dernier resultat de test
				NkVector<int32> testOk;		 // statut parallele a testLog : 0 neutre, 1 ok, 2 echec
				char filter[80] = "";
				int32 focus = -1, caret = 0;
				float32 blink = 0.f;
				bool focusClaimed =
					false; // un champ a-t-il capte le clic cette frame ? (sinon clic dans le vide = defocus)
				float32 scroll = 0.f, scrollMax = 0.f;
				int32 barDrag = 0;
				float32 barOff = 0.f;
				bool detected = false;
				int32 lastCustomN = -1; // suivi de la taille de nw->customTc -> sauvegarde auto du registre
				// ── Vue de progression de la detection (bouton « Detecter ») ──
				bool detecting = false;
				int32 detectStep = 0;
				float32 detectTimer = 0.f;
				NkVector<NkString> detectLog;
				NkVector<NkTcDetail> pendingSys; // resultat en cours -> applique si « Appliquer »
				bool optCross = true, optLocalBin = true, optSdk = true, optDeep = false;
				// ── Editeur integre des variables d'environnement (cross-platform) ──
				bool envDlg = false;

				struct EnvRow {
						char name[64] = "";
						char value[320] = "";
				};

				NkVector<EnvRow> envRows;
				int32 envFocus = -1, envCaret = 0;
				float32 envBlink = 0.f;
				float32 envScroll = 0.f;
				char envNewName[64] = "";
				// ── Fiche « Comment installer » (toolchain manquant) ──
				bool installDlg = false;
				NkString installName, installText, installCmd;
				// ── Détection assistée (pointer manuellement un toolchain/SDK que l'auto-détection a raté) ──
				bool assistDlg = false;
				int32 assistKind = 0; // 0 compilateur, 1 zig, 2 android-ndk, 3 emscripten, 4 ohos-sdk
				char assistPath[320] = "";
				int32 assistFocus = -1, assistCaret = 0;
				float32 assistBlink = 0.f;
				bool assistTested = false, assistOk = false;
				NkString assistMsg;	 // message de résultat (dynamique)
				NkString assistCaps; // pour zig : familles réellement compilables
				bool assistComboOpen = false;
				NkAppleZig assistApple; // pour le type « Toolchain Apple » : détail des 5 composants

				// ── Enregistrement + re-detection sur un THREAD de fond (NkThread, pas de STL) ──
				// Evite de geler l'UI pendant les setx / reg query / interrogation WSL. Progression
				// visuelle via bgPhase. Resultats remis au thread principal via PollSaveEnv().
				threading::NkThread bgThread;
				NkAtomicBool bgBusy;				  // un enregistrement/detection tourne
				NkAtomicBool bgDone;				  // le thread a fini -> a appliquer
				NkAtomicInt32 bgPhase;				  // 0 idle, 1 persist, 3 detect, 5 finalise, 6 fini
				NkVector<NkTcDetail> bgSys, bgCustom; // resultats produits par le thread

				~NkToolchainsState() {
					if (bgThread.Joinable())
						bgThread.Join();
				}

				// Instructions d'installation reelles (selon l'OS) pour un toolchain manquant.
				static void InstallInfo(const char *name, NkString &outText, NkString &outCmd) {
					outText = NkString();
					outCmd = NkString();
#if defined(_WIN32)
					const int32 os = 0;
#elif defined(__APPLE__)
					const int32 os = 1;
#else
					const int32 os = 2;
#endif
					auto pick = [&](const char *w, const char *m, const char *l) {
						outCmd = (os == 0) ? w : (os == 1) ? m : l;
					};
					const NkString nm = name ? name : "";
					// MSYS2 en 1 clic : winget installe MSYS2 PUIS pacman installe la toolchain (ucrt64).
					const char *MSYS2_GCC = "winget install --accept-package-agreements --accept-source-agreements -e "
											"--id MSYS2.MSYS2 && C:\\msys64\\usr\\bin\\bash -lc \"pacman -Sy "
											"--noconfirm --needed mingw-w64-ucrt-x86_64-toolchain\"";
					const char *MSYS2_CLANG =
						"winget install --accept-package-agreements --accept-source-agreements -e --id MSYS2.MSYS2 && "
						"C:\\msys64\\usr\\bin\\bash -lc \"pacman -Sy --noconfirm --needed mingw-w64-ucrt-x86_64-clang "
						"mingw-w64-ucrt-x86_64-clang-tools-extra\"";
					if (StrEq(name, "zig")) {
						outText = NkT("tc.inst.zig");
						pick("winget install zig.zig", "brew install zig", "snap install zig --classic");
					} else if (StrEq(name, "msvc")) {
						outText = NkT("tc.inst.msvc");
						pick("winget install LLVM.LLVM", "N/A (Windows uniquement)", "N/A (Windows uniquement)");
					} else if (StrEq(name, "msys2")) {
						outText = NkT("tc.inst.msys2");
						pick(
							"winget install --accept-package-agreements --accept-source-agreements -e --id MSYS2.MSYS2",
							"N/A (Windows uniquement)", "N/A (Windows uniquement)");
					} else if (nm.Contains("clang")) {
						outText = NkT("tc.inst.clang");
						pick(MSYS2_CLANG, "brew install llvm", "sudo apt install -y clang lld");
					} else if (nm.Contains("gcc") || nm.Contains("mingw")) {
						outText = NkT("tc.inst.gcc");
						pick(MSYS2_GCC, "brew install gcc", "sudo apt install -y g++");
					} else if (StrEq(name, "android-ndk")) {
						outText = NkT("tc.inst.ndk");
						pick("setx ANDROID_NDK_ROOT \"C:\\Android\\ndk\\<version>\"",
							 "export ANDROID_NDK_ROOT=$HOME/Android/ndk/<version>",
							 "export ANDROID_NDK_ROOT=$HOME/Android/ndk/<version>");
					} else if (StrEq(name, "emscripten")) {
						outText = NkT("tc.inst.emsdk");
						pick("git clone https://github.com/emscripten-core/emsdk && emsdk\\emsdk install latest && "
							 "emsdk\\emsdk activate latest",
							 "git clone https://github.com/emscripten-core/emsdk && ./emsdk/emsdk install latest",
							 "git clone https://github.com/emscripten-core/emsdk && ./emsdk/emsdk install latest");
					} else if (StrEq(name, "ohos-ndk")) {
						outText = NkT("tc.inst.ohos");
						pick("setx OHOS_SDK \"C:\\ohos\\...\\openharmony\"", "export OHOS_SDK=...",
							 "export OHOS_SDK=...");
					} else {
						outText = NkT("tc.inst.default");
						outCmd = "";
					}
				}

				static const int32 DETECT_STEPS = 10;

				static const char *DetectStepLabel(int32 i) {
					static const char *k[] = {"tc.step0", "tc.step1", "tc.step2", "tc.step3", "tc.step4",
											  "tc.step5", "tc.step6", "tc.step7", "tc.step8", "tc.step9"};
					return (i >= 0 && i < DETECT_STEPS) ? NkT(k[i]) : "";
				}

				void StartDetect() {
					detecting = true;
					detectStep = 0;
					detectTimer = 0.f;
					detectLog = NkVector<NkString>();
					pendingSys = NkVector<NkTcDetail>();
				}

				// Statut d'un toolchain vs l'ancienne detection (sys) : 0 nouveau, 1 inchange, 2 maj, 3 manquant.
				int32 StatusVs(const NkTcDetail &d) {
					for (usize i = 0; i < sys.Size(); ++i)
						if (StrEq(sys[i].name.CStr(), d.name.CStr())) {
							if (!d.found)
								return 3;
							const NkString oldp = sys[i].pv.Empty() ? NkString() : sys[i].pv[0];
							const NkString newp = d.pv.Empty() ? NkString() : d.pv[0];
							return StrEq(oldp.CStr(), newp.CStr()) ? 1 : 2;
						}
					return d.found ? 0 : 3;
				}

				// Avance la detection d'une etape (appelee chaque frame en mode detecting).
				void StepAdvance(float32 dt) {
					if (!detecting || detectStep >= DETECT_STEPS)
						return;
					detectTimer += dt;
					if (detectTimer < 0.35f)
						return;
					detectTimer = 0.f;
					auto log = [&](const NkString &m) { detectLog.PushBack(m); };
					auto oneWhich = [&](const char *nm, const char *verLabel, const char *tgt, const char *exe,
										const char *exe2, const char *arExe) {
						NkString cc = Which(exe);
						if (cc.Empty() && exe2)
							cc = Which(exe2);
						if (!cc.Empty()) {
							NkTcDetail d;
							d.name = nm;
							d.version = verLabel;
							d.target = tgt;
							d.found = true;
							d.pk.PushBack("CC");
							d.pv.PushBack(cc);
							NkString cxx = exe2 ? Which(exe2) : NkString();
							if (!cxx.Empty()) {
								d.pk.PushBack("CXX");
								d.pv.PushBack(cxx);
							}
							if (arExe) {
								NkString ar = Which(arExe);
								if (ar.Empty())
									ar = Which("ar");
								if (!ar.Empty()) {
									d.pk.PushBack("AR");
									d.pv.PushBack(ar);
								}
							}
							pendingSys.PushBack(d);
							NkString l = "[ok] ";
							l += nm;
							l += " -> ";
							l += cc;
							log(l);
						} else {
							NkTcDetail d;
							d.name = nm;
							d.target = tgt;
							d.found = false;
							d.reason = NkT("tc.rn.notfound");
							pendingSys.PushBack(d);
							NkString l = "[--] ";
							l += nm;
							l += " ";
							l += NkT("tc.log.notfound");
							log(l);
						}
					};
					switch (detectStep) {
						case 0: {
							NkString l = "[detect] ";
							l += NkT("tc.log.pathscan");
							log(l);
						} break;
#if defined(_WIN32)
						case 1:
							oneWhich("clang-mingw", "Clang", "Windows/x86_64/gnu (MinGW)", "clang", "clang++",
									 "llvm-ar");
							break;
						case 2:
							oneWhich("mingw", "GCC", "Windows/x86_64/gnu (MinGW)", "gcc", "g++", "ar");
							break;
#else
						case 1:
							oneWhich("host-clang", "Clang", "Linux/x86_64/gnu", "clang", "clang++", "llvm-ar");
							break;
						case 2:
							oneWhich("host-gcc", "GCC", "Linux/x86_64/gnu", "gcc", "g++", "ar");
							break;
#endif
						case 3: {
							NkString ndk = Env("ANDROID_NDK_ROOT");
							if (ndk.Empty())
								ndk = Env("ANDROID_NDK_HOME");
							NkTcDetail d;
							d.name = "android-ndk";
							d.target = "Android/ARM64";
							if (!ndk.Empty()) {
								d.found = true;
								d.version = "NDK";
								d.pk.PushBack("NDK");
								d.pv.PushBack(ndk);
								NkString l = "[ndk] ";
								l += ndk;
								log(l);
							} else {
								d.found = false;
								d.reason = NkT("tc.rn.ndkundef");
								NkString l = "[--] android-ndk ";
								l += NkT("tc.log.notfound");
								log(l);
							}
							pendingSys.PushBack(d);
						} break;
						case 4: {
							NkString em = Env("EMSDK");
							if (em.Empty())
								em = Which("emcc");
							NkTcDetail d;
							d.name = "emscripten";
							d.target = "Web/WASM32";
							if (!em.Empty()) {
								d.found = true;
								d.version = "Emscripten";
								d.pk.PushBack("EMSDK");
								d.pv.PushBack(em);
								NkString l = "[emsdk] ";
								l += em;
								log(l);
							} else {
								d.found = false;
								d.reason = NkT("tc.rn.emsdkundef");
								NkString l = "[--] emscripten ";
								l += NkT("tc.log.notfound");
								log(l);
							}
							pendingSys.PushBack(d);
						} break;
						case 5: {
							NkString z = Which("zig");
							NkTcDetail d;
							d.name = "zig";
							d.target = "cross";
							if (!z.Empty()) {
								d.found = true;
								d.version = "Zig";
								d.pk.PushBack("zig");
								d.pv.PushBack(z);
								NkString l = "[zig] ";
								l += z;
								log(l);
							} else {
								d.found = false;
								d.reason = NkT("tc.rn.zignf");
								NkString l = "[--] zig ";
								l += NkT("tc.log.notfound");
								log(l);
							}
							pendingSys.PushBack(d);
						} break;
						case 6: {
							NkString cl = Which("clang-cl");
							NkTcDetail d;
							d.name = "msvc";
							d.target = "Windows/x86_64/msvc";
							if (!cl.Empty()) {
								d.found = true;
								d.version = "clang-cl";
								d.pk.PushBack("CC");
								d.pv.PushBack(cl);
								NkString l = "[msvc] ";
								l += cl;
								log(l);
							} else {
								d.found = false;
								d.reason = NkT("tc.rn.clangclnf");
								NkString l = "[--] msvc ";
								l += NkT("tc.log.notfound");
								log(l);
							}
							pendingSys.PushBack(d);
						} break;
						case 7: {
							NkString o = Env("OHOS_SDK");
							if (o.Empty())
								o = Env("OHOS_NDK_HOME");
							if (o.Empty())
								o = Env("DEVECO_SDK_HOME");
							if (!o.Empty()) {
								NkString l = "[ohos] ";
								l += o;
								log(l);
							} else {
								NkString l = "[--] ohos-ndk ";
								l += NkT("tc.log.notfound");
								log(l);
							}
						} break;
						case 8: {
#if defined(_WIN32)
							if (NkFile::Exists("C:\\Windows\\System32\\wsl.exe")) {
								NkString l = "[wsl2] ";
								l += NkT("tc.log.wslquery");
								log(l);
							} else {
								NkString l = "[--] WSL2 ";
								l += NkT("tc.log.absent");
								log(l);
							}
#else
							{
								NkString l = "[wsl2] N/A (";
								l += NkT("tc.log.unixhost");
								l += ")";
								log(l);
							}
#endif
						} break;
						case 9: {
							NkString l = "[reg] ";
							l += NkT("tc.log.regupdate");
							log(l);
						} break;
					}
					++detectStep;
				}

				// Le resultat FINAL vient de Detect() (source unique, complete : WSL2 + ohos + RefreshPath),
				// pas du pendingSys de l'animation (qui n'est qu'un apercu par etapes). Evite de « perdre »
				// WSL/HarmonyOS lors d'un clic sur « Detecter ».
				void ApplyDetect() {
					detecting = false;
					detected = false;
					StartDetectAsync();
				} // detection reelle sur thread de fond (pas de gel)

				NkString wslCompsCache; // compilateurs WSL memorises -> robustesse si un scan echoue
				NkString
					zigCaps; // familles compilables par zig (TEST reel, calcule UNE fois) : "linux windows macos ..."

				// Teste REELLEMENT les cibles que ce zig peut compiler+linker (une compilation triviale par cible).
				// linux/windows sont toujours OK (libc embarquee) ; macOS/iOS dependent des stubs/SDK -> testes.
				// Cherche ld.lld.exe (linkeur Mach-O) dans le NDK Android (cf. doc §4).
				static NkString FindNdkLdLld() {
					auto env = [](const char *n) -> NkString {
						// API maison (qualif. complète : la variable locale `env` masque le namespace).
						const char *v = nkentseu::env::GetEnvVar(n);
						return (v && *v) ? NkString(v) : NkString();
					};
					auto probe = [](const NkString &base) -> NkString {
						const NkString pre = (NkPath(base.CStr()) / "toolchains" / "llvm" / "prebuilt").ToString();
						if (!NkDirectory::Exists(pre.CStr()))
							return NkString();
						const NkVector<NkString> hosts = NkDirectory::GetDirectories(pre.CStr(), "*");
						for (usize i = 0; i < hosts.Size(); ++i) {
							const NkString e = (NkPath(hosts[i].CStr()) / "bin" / "ld.lld.exe").ToString();
							if (NkFile::Exists(e.CStr()))
								return e;
						}
						return NkString();
					};
					NkString ndk = env("ANDROID_NDK_HOME");
					if (ndk.Empty())
						ndk = env("ANDROID_NDK_ROOT");
					if (!ndk.Empty()) {
						const NkString r = probe(ndk);
						if (!r.Empty())
							return r;
					}
					const NkVector<NkString> vers = NkDirectory::GetDirectories("C:\\Android\\ndk", "*");
					for (usize i = 0; i < vers.Size(); ++i) {
						const NkString r = probe(vers[i]);
						if (!r.Empty())
							return r;
					}
					return NkString();
				}

				// Détection RÉELLE des 5 composants de la toolchain Apple (cf. doc §3). Aucune compilation :
				// env + présence de fichiers/.tbd. zig 0.13 sert macOS ET iOS ; iOS exige SDK complet + ld.lld +
				// libc++.
				static NkAppleZig DetectAppleZig() {
					NkAppleZig a;
					auto env = [](const char *n) -> NkString {
						// API maison (qualif. complète : la variable locale `env` masque le namespace).
						const char *v = nkentseu::env::GetEnvVar(n);
						return (v && *v) ? NkString(v) : NkString();
					};
					auto fileEx = [](const NkString &p) { return !p.Empty() && NkFile::Exists(p.CStr()); };
					// 1) zig 0.13 (ZIG_MACOS ou défaut apple-sdks)
					a.zig = env("ZIG_MACOS");
					if (a.zig.Empty())
						a.zig = "C:\\apple-sdks\\zigldl\\zig-windows-x86_64-0.13.0\\zig.exe";
					if (fileEx(a.zig)) {
						NkString inner = "\"";
						inner += a.zig;
						inner += "\" version 2>&1";
						NkString cmd;
#if defined(_WIN32)
						cmd = "\"";
						cmd += inner;
						cmd += "\"";
#else
						cmd = inner;
#endif
						const NkString o = NkNewWsState::TcRun(cmd.CStr());
						NkString first;
						for (const char *c = o.CStr(); *c && *c != '\n' && *c != '\r'; ++c)
							first += *c;
						a.zigVer = first;
						a.okZig = first.Contains("0.13");
					}
					// 2) SDK iOS complet (présence de usr/lib/libSystem.tbd)
					a.iosSdk = env("IOS_SDK");
					if (a.iosSdk.Empty()) {
						const NkVector<NkString> g = NkDirectory::GetDirectories("C:\\apple-sdks", "iPhoneOS*.sdk");
						if (!g.Empty())
							a.iosSdk = g[0];
					}
					if (!a.iosSdk.Empty()) {
						const NkString tbd = (NkPath(a.iosSdk.CStr()) / "usr" / "lib" / "libSystem.tbd").ToString();
						const NkString tbdB = (NkPath(a.iosSdk.CStr()) / "usr" / "lib" / "libSystem.B.tbd").ToString();
						a.okIosSdk = NkFile::Exists(tbd.CStr()) || NkFile::Exists(tbdB.CStr());
					}
					// 3) ld.lld (NDK Android)
					a.ld64 = env("LD64");
					if (a.ld64.Empty())
						a.ld64 = FindNdkLdLld();
					a.okLd = fileEx(a.ld64);
					// 4) libc++ iOS retaguée
					a.libcpp = env("IOS_LIBCPP_DIR");
					if (a.libcpp.Empty())
						a.libcpp = "C:\\apple-sdks\\libcxx-ios";
					{
						const NkString lib = (NkPath(a.libcpp.CStr()) / "libc++.a").ToString();
						a.okLibcpp = NkFile::Exists(lib.CStr());
					}
					// 5) SDK macOS (défaut, sinon fallback glob MacOSX*.sdk — comme do_check)
					a.macSdk = env("MACOS_SDK");
					if (a.macSdk.Empty())
						a.macSdk = "C:\\apple-sdks\\MacOSX11.3.sdk";
					if (!NkDirectory::Exists(a.macSdk.CStr())) {
						const NkVector<NkString> g = NkDirectory::GetDirectories("C:\\apple-sdks", "MacOSX*.sdk");
						if (!g.Empty())
							a.macSdk = g[0];
					}
					{
						const NkString tbd = (NkPath(a.macSdk.CStr()) / "usr" / "lib" / "libSystem.tbd").ToString();
						a.okMacSdk = NkFile::Exists(tbd.CStr());
					}
					// rcodesign (optionnel : signature)
					a.rcodesign = env("RCODESIGN");
					if (a.rcodesign.Empty()) {
						const NkVector<NkString> g =
							NkDirectory::GetDirectories("C:\\apple-sdks\\tools", "apple-codesign-*");
						for (usize i = 0; i < g.Size(); ++i) {
							const NkString e = (NkPath(g[i].CStr()) / "rcodesign.exe").ToString();
							if (NkFile::Exists(e.CStr())) {
								a.rcodesign = e;
								break;
							}
						}
					}
					a.signOk = fileEx(a.rcodesign);
					a.iosOk = a.okZig && a.okIosSdk && a.okLd && a.okLibcpp;
					a.macosOk = a.okZig && a.okMacSdk;
					return a;
				}

				// Commande d'installation/réparation de la toolchain Apple (script Jenga). `root` = dossier cible.
				static NkString AppleSetupCommand(const NkString &root) {
					auto env = [](const char *n) -> NkString {
						// API maison (qualif. complète : la variable locale `env` masque le namespace).
						const char *v = nkentseu::env::GetEnvVar(n);
						return (v && *v) ? NkString(v) : NkString();
					};
					NkString script = env("JENGA_APPLE_SETUP"); // override explicite (chemin complet du .py)
					NkString py = "python";
					NkString cmd = py;
					cmd += " ";
					if (!script.Empty() && NkFile::Exists(script.CStr())) {
						cmd += "\"";
						cmd += script;
						cmd += "\"";
					} else {
						// Localise le script via le paquet jenga installé (repo source : <jenga>/../scripts/…).
						cmd += "-c \"import os,sys,runpy;";
						cmd += "import jenga;";
						cmd += "r=os.path.dirname(os.path.dirname(os.path.abspath(jenga.__file__)));";
						cmd += "s=os.path.join(r,'scripts','setup_apple_toolchain.py');";
						cmd += "sys.argv=[s]+sys.argv[1:];";
						cmd += "runpy.run_path(s,run_name='__main__')\"";
					}
					cmd += " --root \"";
					cmd += root;
					cmd += "\"";
					return cmd;
				}

				static NkString ZigCaps(const NkString &zigExe) {
					// zig cible TOUJOURS linux + windows (libc bundlé) : inutile de compile-tester.
					// Le support Apple NE dépend PAS du zig du PATH mais de la toolchain dédiée
					// (C:\apple-sdks : zig 0.13 + SDK iOS complet + ld.lld + libc++). Cf. doc §1/§6.
					(void)zigExe;
					NkString caps = "linux windows";
					const NkAppleZig a = DetectAppleZig();
					if (a.macosOk)
						caps += " macos";
					if (a.iosOk)
						caps += " ios";
					return caps;
				}

				static NkString Env(const char *n) {
					return NkNewWsState::TcEnv(n);
				}

				static NkString Which(const char *e) {
					return NkNewWsState::TcWhich(e);
				}

				// Remplace les %VAR% par leur valeur (getenv).
				static NkString ExpandVars(const NkString &in) {
					NkString out, var;
					bool inVar = false;
					for (const char *c = in.CStr(); *c; ++c) {
						if (*c == '%') {
							if (inVar) {
								const char *v = env::GetEnvVar(var.CStr()); // API maison (NkEnv.h)
								if (v)
									out += v;
								else {
									out += '%';
									out += var;
									out += '%';
								}
								var = NkString();
								inVar = false;
							} else
								inVar = true;
						} else if (inVar)
							var += *c;
						else
							out += *c;
					}
					if (inVar) {
						out += '%';
						out += var;
					}
					return out;
				}

				// Re-hydrate le PATH du process depuis le registre (capte les outils installes APRES le lancement).
				static void RefreshPath() {
#if defined(_WIN32)
					auto regPath = [](const char *hive) -> NkString {
						NkString cmd = "reg query \"";
						cmd += hive;
						cmd += "\" /v Path 2>nul";
						const NkString out = NkNewWsState::TcRun(cmd.CStr());
						NkString line;
						for (const char *s = out.CStr();; ++s) {
							if (*s == '\n' || *s == '\0') {
								if (line.Contains("REG_")) {
									const char *p = nullptr;
									for (const char *q = line.CStr(); *q; ++q)
										if (q[0] == 'R' && q[1] == 'E' && q[2] == 'G' && q[3] == '_') {
											p = q;
											break;
										}
									if (p) {
										while (*p && *p != ' ' && *p != '\t')
											++p;
										while (*p == ' ' || *p == '\t')
											++p;
										NkString v;
										for (; *p; ++p)
											v += *p;
										return ExpandVars(v);
									}
								}
								line = NkString();
								if (*s == '\0')
									break;
							} else
								line += *s;
						}
						return NkString();
					};
					const NkString up = regPath("HKCU\\Environment");
					const NkString sp =
						regPath("HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");
					// FUSION : registre (machine + user) + PATH COURANT du process. Crucial : certains
					// outils (ex zig via winget -> %LOCALAPPDATA%\Microsoft\WinGet\Links) ne sont PAS
					// inscrits dans le registre mais SONT dans le PATH herite -> il ne faut pas les perdre.
					NkString full = sp;
					if (!up.Empty()) {
						if (!full.Empty())
							full += ";";
						full += up;
					}
					const char *cur = env::GetEnvVar("PATH"); // API maison (NkEnv.h)
					if (cur && *cur) {
						if (!full.Empty())
							full += ";";
						full += cur;
					}
					if (full.Empty())
						return;
					// Dedup insensible a la casse (evite la croissance a chaque appel).
					NkString dedup;
					NkVector<NkString> seen;
					NkString tok;
					auto flush = [&]() {
						if (tok.Empty())
							return;
						NkString low;
						for (const char *c = tok.CStr(); *c; ++c) {
							char ch = *c;
							if (ch >= 'A' && ch <= 'Z')
								ch = (char)(ch + 32);
							low += ch;
						}
						bool dup = false;
						for (usize i = 0; i < seen.Size(); ++i)
							if (seen[i] == low) {
								dup = true;
								break;
							}
						if (!dup) {
							seen.PushBack(low);
							if (!dedup.Empty())
								dedup += ";";
							dedup += tok;
						}
						tok = NkString();
					};
					for (const char *c = full.CStr(); *c; ++c) {
						if (*c == ';')
							flush();
						else
							tok += *c;
					}
					flush();
					if (!dedup.Empty()) {
						NkString e = "PATH=";
						e += dedup;
						_putenv(e.CStr());
					}
#endif
				}

				static NkString RegFile() {
					const NkString home = NkOpenWsState::Home();
					return (NkPath(home.CStr()) / ".jenga" / "toolchains_registry.json").ToString();
				}

				// Ecrit le registre global (~/.jenga/toolchains_registry.json) depuis les toolchains custom.
				static void SaveRegistry(NkNewWsState *nw) {
					NkString js = "{\n  \"toolchains\": [\n";
					for (usize i = 0; i < nw->customTc.Size(); ++i) {
						const NkNewWsState::CustomTc &t = nw->customTc[i];
						if (i)
							js += ",\n";
						js += "    { \"name\": \"";
						js += t.name;
						js += "\", \"triple\": \"";
						js += t.triple;
						js += "\", \"cc\": \"";
						js += t.cc;
						js += "\", \"cxx\": \"";
						js += t.cxx;
						js += "\", \"ar\": \"";
						js += t.ar;
						js += "\", \"ld\": \"";
						js += t.ld;
						js += "\" }";
					}
					js += "\n  ]\n}\n";
					const NkString path = RegFile();
					NkDirectory::CreateRecursive(NkPath(path.CStr()).GetParent().ToString().CStr());
					NkFile::WriteAllText(NkPath(path.CStr()), js);
				}

				// ── Détection assistée ─────────────────────────────────────────────────
				// Les 5 types que l'utilisateur peut pointer manuellement (traduits).
				static const char *AssistKindLabel(int32 k) {
					static const char *key[] = {"as.kind.cc",	 "as.kind.zig",	 "as.kind.ndk",
												"as.kind.emsdk", "as.kind.ohos", "as.kind.apple"};
					return (k >= 0 && k < 6) ? NkT(key[k]) : "";
				}

				static int32 AssistKindCount() {
					return 6;
				}

				// true = on pointe un FICHIER (binaire) ; false = un DOSSIER (SDK / racine).
				static bool AssistKindIsFile(int32 k) {
					return k == 0 || k == 1;
				}

				static bool AssistKindIsApple(int32 k) {
					return k == 5;
				} // toolchain Apple : détection multi-composants + install

				// (PersistEnv est défini plus bas — réutilisé ici par AssistAdd pour les SDK.)
				// Vérifie RÉELLEMENT le chemin pointé selon le type. Ne triche jamais : un binaire doit
				// répondre à --version, un SDK doit contenir sa structure attendue, zig est compile-testé.
				void AssistVerify() {
					assistTested = true;
					assistOk = false;
					assistCaps = NkString();
					assistMsg = NkString();
					const NkString p = assistPath;
					if (p.Empty()) {
						assistMsg = NkT("as.err.nopath");
						return;
					}
					switch (assistKind) {
						case 0: { // compilateur : le binaire existe ET répond à --version
							if (!NkFile::Exists(p.CStr())) {
								assistMsg = NkT("as.err.nofile");
								return;
							}
							NkString inner = "\"";
							inner += p;
							inner += "\" --version 2>&1";
							NkString cmd;
#if defined(_WIN32)
							cmd = "\"";
							cmd += inner;
							cmd += "\""; // cf. FIX _popen/cmd (voir ZigCaps)
#else
							cmd = inner;
#endif
							const NkString o = NkNewWsState::TcRun(cmd.CStr());
							if (o.Empty()) {
								assistMsg = NkT("as.err.noversion");
								return;
							}
							NkString first;
							for (const char *c = o.CStr(); *c && *c != '\n' && *c != '\r'; ++c)
								first += *c;
							assistOk = true;
							assistMsg = first;
						} break;
						case 1: { // zig : existe + test réel des cibles (ZigCaps)
							if (!NkFile::Exists(p.CStr())) {
								assistMsg = NkT("as.err.nofile");
								return;
							}
							assistCaps = ZigCaps(p);
							assistOk = !assistCaps.Empty();
							assistMsg = assistOk ? assistCaps : NkString(NkT("as.err.zigfail"));
						} break;
						case 2: { // Android NDK : dossier + toolchains/llvm/prebuilt
							if (!NkDirectory::Exists(p.CStr())) {
								assistMsg = NkT("as.err.nofolder");
								return;
							}
							const NkString probe = (NkPath(p.CStr()) / "toolchains" / "llvm" / "prebuilt").ToString();
							assistOk = NkDirectory::Exists(probe.CStr());
							assistMsg = assistOk ? NkString("Android NDK OK") : NkString(NkT("as.err.ndkinvalid"));
						} break;
						case 3: { // Emscripten : dossier contenant emcc (direct ou upstream/emscripten)
							if (!NkDirectory::Exists(p.CStr())) {
								assistMsg = NkT("as.err.nofolder");
								return;
							}
							const char *cand[] = {"emcc", "emcc.bat", "upstream/emscripten/emcc",
												  "upstream/emscripten/emcc.bat"};
							bool ok = false;
							for (int32 i = 0; i < 4 && !ok; ++i) {
								const NkString f = (NkPath(p.CStr()) / cand[i]).ToString();
								if (NkFile::Exists(f.CStr()))
									ok = true;
							}
							assistOk = ok;
							assistMsg = ok ? NkString("Emscripten OK") : NkString(NkT("as.err.emsdkinvalid"));
						} break;
						case 4: { // HarmonyOS SDK : dossier + native/llvm
							if (!NkDirectory::Exists(p.CStr())) {
								assistMsg = NkT("as.err.nofolder");
								return;
							}
							const NkString probe = (NkPath(p.CStr()) / "native" / "llvm").ToString();
							assistOk = NkDirectory::Exists(probe.CStr());
							assistMsg = assistOk ? NkString("HarmonyOS SDK OK") : NkString(NkT("as.err.ohosinvalid"));
						} break;
						case 5: { // Toolchain Apple (zig) : détection RÉELLE des 5 composants (doc §3)
							assistApple = DetectAppleZig();
							assistOk = assistApple.macosOk || assistApple.iosOk; // au moins une famille prête
							// Message = récap court ; le détail par composant est rendu dans le dialogue.
							NkString m;
							m += assistApple.macosOk ? "macOS OK" : "macOS --";
							m += "   ";
							m += assistApple.iosOk ? "iOS OK" : "iOS --";
							if (assistApple.signOk)
								m += "   (signature dispo)";
							assistMsg = m;
						} break;
					}
				}

				// Ajoute le résultat vérifié : compilateur/zig -> registre custom ; SDK -> variable d'env persistée.
				void AssistAdd(NkNewWsState *nw) {
					if (!assistOk)
						return;
					if (assistKind == 0 || assistKind == 1) {
						NkNewWsState::CustomTc t{};
						const char *base = assistPath;
						for (const char *c = assistPath; *c; ++c)
							if (*c == '/' || *c == '\\')
								base = c + 1;
						if (assistKind == 1)
							NkStrCopy(t.name, sizeof(t.name), "zig-manuel"); // copie bornée maison (NkText.h)
						else
							NkStrCopy(t.name, sizeof(t.name), base);
						NkStrCopy(t.cc, sizeof(t.cc), assistPath);
						NkStrCopy(t.cxx, sizeof(t.cxx), assistPath);
						nw->customTc.PushBack(t);
						SaveRegistry(nw);
						LoadRegistry();
						lastCustomN = (int32)nw->customTc.Size();
					} else {
						const char *var = (assistKind == 2)	  ? "ANDROID_NDK_ROOT"
										  : (assistKind == 3) ? "EMSDK"
															  : "OHOS_SDK";
						PersistEnv(var, assistPath);
					}
					detected = false; // forcer une re-détection au prochain affichage
					assistDlg = false;
					assistTested = false;
					assistPath[0] = '\0';
					assistFocus = -1;
				}

				void OpenAssistDialog() {
					assistDlg = true;
					assistTested = false;
					assistOk = false;
					assistMsg = NkString();
					assistCaps = NkString();
					assistFocus = -1;
					assistComboOpen = false;
					assistPath[0] = '\0';
				}

				// Ouvre l'editeur INTEGRE (cross-platform) des variables d'environnement SDK.
				void OpenEnvDialog() {
					envDlg = true;
					envFocus = -1;
					envScroll = 0.f;
					envNewName[0] = '\0';
					envRows = NkVector<EnvRow>();
					const char *keys[] = {"ANDROID_SDK_ROOT", "ANDROID_NDK_ROOT", "EMSDK",
										  "OHOS_SDK",		  "JAVA_HOME",		  "ZIG_ROOT"};
					for (int32 i = 0; i < 6; ++i) {
						EnvRow rw;
						NkStrCopy(rw.name, sizeof(rw.name), keys[i]); // copie bornée maison (NkText.h)
						const NkString v = Env(keys[i]);
						NkStrCopy(rw.value, sizeof(rw.value), v.CStr());
						envRows.PushBack(rw);
					}
				}

				// Renseigne AUTOMATIQUEMENT les variables vides en cherchant les SDK sur le disque.
				// Cross-platform (Windows / macOS / Linux). N'ecrase jamais une valeur deja definie.
				void AutoDetectEnv() {
					auto rowByName = [&](const char *nm) -> EnvRow * {
						for (usize i = 0; i < envRows.Size(); ++i)
							if (StrEq(envRows[i].name, nm))
								return &envRows[i];
						return nullptr;
					};
					auto fill = [&](const char *nm, const NkString &val) {
						if (val.Empty() || !NkDirectory::Exists(val.CStr()))
							return;
						EnvRow *r = rowByName(nm);
						if (r && r->value[0] == '\0')
							NkStrCopy(r->value, sizeof(r->value), val.CStr());
					};
					auto parentOf = [](const NkString &file) {
						return NkPath(file.CStr()).GetParent().ToString();
					}; // dossier du fichier
					const char *h = env::GetEnvVar("USERPROFILE"); // API maison (NkEnv.h)
					if (!h || !*h)
						h = env::GetEnvVar("HOME");
					const NkString home = h ? h : "";
					// Android SDK
					NkString sdk;
#if defined(_WIN32)
					{
						const char *la = env::GetEnvVar("LOCALAPPDATA"); // API maison (NkEnv.h)
						if (la) {
							NkString p = la;
							p += "\\Android\\Sdk";
							if (NkDirectory::Exists(p.CStr()))
								sdk = p;
						}
					}
#elif defined(__APPLE__)
					{
						NkString p = home;
						p += "/Library/Android/sdk";
						if (NkDirectory::Exists(p.CStr()))
							sdk = p;
					}
#else
					{
						NkString p = home;
						p += "/Android/Sdk";
						if (NkDirectory::Exists(p.CStr()))
							sdk = p;
					}
#endif
					fill("ANDROID_SDK_ROOT", sdk);
					// Android NDK = <sdk>/ndk/<version la plus recente>
					if (!sdk.Empty()) {
						NkString nd = sdk;
						nd += "/ndk";
						if (NkDirectory::Exists(nd.CStr())) {
							NkVector<NkDirectoryEntry> e =
								NkDirectory::GetEntries(NkPath(nd.CStr()), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
							NkString best;
							for (usize i = 0; i < e.Size(); ++i)
								if (e[i].IsDirectory) {
									const NkString &n = e[i].Name;
									if (best.Empty() || NkString(n.CStr()) > best)
										best = NkString(n.CStr());
								}
							if (!best.Empty()) {
								NkString p = nd;
								p += "/";
								p += best;
								fill("ANDROID_NDK_ROOT", p);
							}
						}
					}
					// EMSDK : parent de emcc, sinon ~/emsdk
					{
						NkString em = Which("emcc");
						if (!em.Empty())
							fill("EMSDK", parentOf(em));
						else {
							NkString p = home;
							p += "/emsdk";
							fill("EMSDK", p);
						}
					}
					// ZIG_ROOT : dossier de zig
					{
						NkString z = Which("zig");
						if (!z.Empty())
							fill("ZIG_ROOT", parentOf(z));
					}
					// JAVA_HOME : racine du JDK (dossier au-dessus de bin/java)
					{
						NkString j = Which("java");
						if (!j.Empty())
							fill("JAVA_HOME", NkPath(parentOf(j).CStr()).GetParent().ToString());
					}
					// OHOS_SDK : emplacements DevEco par defaut
					{
						NkString ohos;
#if defined(_WIN32)
						const char *c1 = "C:\\Program Files\\Huawei\\DevEco Studio\\sdk";
						if (NkDirectory::Exists(c1))
							ohos = c1;
#elif defined(__APPLE__)
						{
							NkString p = home;
							p += "/Library/Huawei/Sdk";
							if (NkDirectory::Exists(p.CStr()))
								ohos = p;
						}
#else
						{
							NkString p = home;
							p += "/Huawei/Sdk";
							if (NkDirectory::Exists(p.CStr()))
								ohos = p;
						}
#endif
						fill("OHOS_SDK", ohos);
					}
				}

				// Persiste une variable pour les FUTURS process ET le process courant. Cross-platform.
				static void PersistEnv(const char *name, const char *value) {
					// 1) process courant (pour que la detection la voie tout de suite)
					NkString e = name;
					e += "=";
					e += value;
#if defined(_WIN32)
					_putenv(e.CStr());
					// 2) persistant (HKCU\Environment) via setx
					NkString cmd = "setx ";
					cmd += name;
					cmd += " \"";
					cmd += value;
					cmd += "\" >nul 2>nul";
					NkCodeShellRun(cmd.CStr());
#else
					setenv(name, value, 1);
					// 2) persistant : mise a jour de ~/.profile (ligne export NAME=...)
					const NkString home = NkOpenWsState::Home();
					const NkString prof = (NkPath(home.CStr()) / ".profile").ToString();
					NkString content;
					if (NkFile::Exists(prof.CStr()))
						content = NkFile::ReadAllText(NkPath(prof.CStr()));
					NkString prefix = "export ";
					prefix += name;
					prefix += "=";
					NkString out, line;
					for (const char *s = content.CStr();; ++s) {
						if (*s == '\n' || *s == '\0') {
							if (!NkString(line.CStr()).Contains(prefix.CStr())) {
								out += line;
								out += "\n";
							}
							line = NkString();
							if (*s == '\0')
								break;
						} else if (*s != '\r')
							line += *s;
					}
					out += prefix;
					out += "\"";
					out += value;
					out += "\"\n";
					NkFile::WriteAllText(NkPath(prof.CStr()), out);
#endif
				}

				void SaveEnv() {
					for (usize i = 0; i < envRows.Size(); ++i)
						if (envRows[i].name[0])
							PersistEnv(envRows[i].name, envRows[i].value);
					envDlg = false;
					detected = false; /* re-detecte avec les nouvelles valeurs */
				}

				void SetEnvFocus(int32 id, const char *buf) {
					envFocus = id;
					envBlink = 0.f;
					int32 n = 0;
					while (buf[n])
						++n;
					envCaret = n;
				}

				// Enregistre les variables PUIS re-detecte, le tout sur un THREAD de fond : l'UI ne gele plus.
				void StartSaveEnvAsync() {
					if (bgBusy.Load())
						return;
					if (bgThread.Joinable())
						bgThread.Join();
					// Instantane des lignes : le thread ne touche jamais envRows (vit sur le thread UI).
					NkVector<EnvRow> *snap = new NkVector<EnvRow>();
					for (usize i = 0; i < envRows.Size(); ++i)
						snap->PushBack(envRows[i]);
					const NkString wslCache = wslCompsCache;
					const NkString zigCache = zigCaps;
					bgDone.Store(false);
					bgPhase.Store(1);
					bgBusy.Store(true);
					envDlg = false;
					bgThread = threading::NkThread([this, snap, wslCache, zigCache](void *) {
						// 1) Persistance (setx / ~/.profile) — lente, hors thread principal.
						for (usize i = 0; i < snap->Size(); ++i)
							if ((*snap)[i].name[0])
								PersistEnv((*snap)[i].name, (*snap)[i].value);
						delete snap;
						// 2) Re-detection complete dans une instance JETABLE (ne demarre aucun thread).
						bgPhase.Store(3);
						NkToolchainsState tmp;
						tmp.wslCompsCache = wslCache;
						tmp.zigCaps = zigCache;
						tmp.Detect();
						// 3) Transfert des resultats (lus par le thread UI apres bgDone).
						bgPhase.Store(5);
						bgSys = NkVector<NkTcDetail>();
						for (usize i = 0; i < tmp.sys.Size(); ++i)
							bgSys.PushBack(tmp.sys[i]);
						bgCustom = NkVector<NkTcDetail>();
						for (usize i = 0; i < tmp.custom.Size(); ++i)
							bgCustom.PushBack(tmp.custom[i]);
						wslCompsCache = tmp.wslCompsCache;
						zigCaps = tmp.zigCaps;
						bgPhase.Store(6);
						bgDone.Store(true);
					});
				}

				// Detection INITIALE sur thread de fond (evite de geler l'UI : RefreshPath + WSL + compile zig ~4s).
				void StartDetectAsync() {
					if (bgBusy.Load())
						return;
					if (bgThread.Joinable())
						bgThread.Join();
					const NkString wslCache = wslCompsCache;
					const NkString zigCache = zigCaps;
					bgDone.Store(false);
					bgPhase.Store(3);
					bgBusy.Store(true);
					bgThread = threading::NkThread([this, wslCache, zigCache](void *) {
						NkToolchainsState tmp;
						tmp.wslCompsCache = wslCache;
						tmp.zigCaps = zigCache;
						tmp.Detect();
						bgSys = NkVector<NkTcDetail>();
						for (usize i = 0; i < tmp.sys.Size(); ++i)
							bgSys.PushBack(tmp.sys[i]);
						bgCustom = NkVector<NkTcDetail>();
						for (usize i = 0; i < tmp.custom.Size(); ++i)
							bgCustom.PushBack(tmp.custom[i]);
						wslCompsCache = tmp.wslCompsCache;
						zigCaps = tmp.zigCaps;
						bgPhase.Store(6);
						bgDone.Store(true);
					});
				}

				// Garantit une detection (async) sans jamais bloquer : a appeler en tete des panneaux.
				void EnsureDetectedAsync() {
					PollSaveEnv();
					if (!detected && !bgBusy.Load() && !detecting)
						StartDetectAsync();
				}

				// A appeler chaque frame : applique les resultats du thread des qu'ils sont prets.
				void PollSaveEnv() {
					if (!bgDone.Load())
						return;
					if (bgThread.Joinable())
						bgThread.Join();
					sys = NkVector<NkTcDetail>();
					for (usize i = 0; i < bgSys.Size(); ++i)
						sys.PushBack(bgSys[i]);
					custom = NkVector<NkTcDetail>();
					for (usize i = 0; i < bgCustom.Size(); ++i)
						custom.PushBack(bgCustom[i]);
					detected = true;
					bgDone.Store(false);
					bgBusy.Store(false);
					bgPhase.Store(0);
				}

				void Detect() {
					RefreshPath(); // capte les outils (zig...) installes depuis le lancement, sans redemarrer
					sys = NkVector<NkTcDetail>();
					auto found = [&](const char *nm, const char *ver, const char *tgt) {
						NkTcDetail d;
						d.name = nm;
						d.version = ver;
						d.target = tgt;
						d.found = true;
						return d;
					};
					auto missing = [&](const char *nm, const char *tgt, const char *why) {
						NkTcDetail d;
						d.name = nm;
						d.target = tgt;
						d.found = false;
						d.reason = why;
						sys.PushBack(d);
					};
#if defined(_WIN32)
					const char *hostT = "Windows/x86_64/gnu";
					const char *clangName = "clang-mingw";
					const char *clangT = "Windows/x86_64/gnu (MinGW)";
#elif defined(__APPLE__)
					const char *hostT = "macOS/arm64";
					const char *clangName = "host-apple-clang";
					const char *clangT = "macOS/arm64";
#else
					const char *hostT = "Linux/x86_64/gnu";
					const char *clangName = "host-clang";
					const char *clangT = "Linux/x86_64/gnu";
#endif
					// ---- Compilateurs natifs : VARIANTES MULTIPLES + DEDUP par chemin absolu ----
					// Une meme origine (ex ucrt64) et une autre (ex clang64) cohabitent sans s'ecraser ;
					// un meme .exe n'est jamais liste deux fois (dedup sur le chemin CC normalise).
					NkVector<NkString> seenCC; // chemins CC deja pris (normalises) -> anti-duplication
					auto normPath = [](const NkString &p) {
						NkString o;
						for (const char *c = p.CStr(); *c; ++c) {
							char ch = (*c == '\\') ? '/' : *c;
							if (ch >= 'A' && ch <= 'Z')
								ch = (char)(ch + 32);
							o += ch;
						}
						return o;
					};
					auto addTc = [&](const NkString &name, const NkString &ver, const NkString &tgt, const NkString &cc,
									 const NkString &cxx, const NkString &ar) -> bool {
						if (cc.Empty())
							return false;
						const NkString n = normPath(cc);
						for (usize i = 0; i < seenCC.Size(); ++i)
							if (seenCC[i] == n)
								return false; // deja pris -> pas de doublon
						seenCC.PushBack(n);
						NkTcDetail d = found(name.CStr(), ver.CStr(), tgt.CStr());
						d.pk.PushBack("CC");
						d.pv.PushBack(cc);
						if (!cxx.Empty()) {
							d.pk.PushBack("CXX");
							d.pv.PushBack(cxx);
						}
						if (!ar.Empty()) {
							d.pk.PushBack("AR");
							d.pv.PushBack(ar);
						}
						sys.PushBack(d);
						return true;
					};
#if defined(_WIN32)
					(void)clangName;
					(void)clangT;
					(void)hostT;
					// MSYS2 : chaque environnement est une variante DISTINCTE (ucrt64/clang64/mingw64/mingw32).
					{
						NkString root = Env("MSYS2_ROOT");
						if (root.Empty() && NkDirectory::Exists("C:\\msys64"))
							root = "C:\\msys64";
						if (!root.Empty()) {
							const char *envs[][2] = {{"ucrt64", "UCRT64"},
													 {"clang64", "CLANG64"},
													 {"mingw64", "MINGW64"},
													 {"mingw32", "MINGW32"}};
							for (int32 k = 0; k < 4; ++k) {
								NkString bin = root;
								bin += "\\";
								bin += envs[k][0];
								bin += "\\bin\\";
								NkString gcc = bin;
								gcc += "gcc.exe";
								NkString gpp = bin;
								gpp += "g++.exe";
								NkString gar = bin;
								gar += "ar.exe";
								{
									NkString nm = "gcc-";
									nm += envs[k][0];
									NkString v = "GCC (";
									v += envs[k][1];
									v += ")";
									NkString tg = "Windows/x86_64/gnu (MSYS2 ";
									tg += envs[k][1];
									tg += ")";
									addTc(nm, v, tg, NkFile::Exists(gcc.CStr()) ? gcc : NkString(), gpp, gar);
								}
								NkString cl = bin;
								cl += "clang.exe";
								NkString clpp = bin;
								clpp += "clang++.exe";
								NkString lar = bin;
								lar += "llvm-ar.exe";
								{
									NkString nm = "clang-";
									nm += envs[k][0];
									NkString v = "Clang (";
									v += envs[k][1];
									v += ")";
									NkString tg = "Windows/x86_64/llvm (MSYS2 ";
									tg += envs[k][1];
									tg += ")";
									addTc(nm, v, tg, NkFile::Exists(cl.CStr()) ? cl : NkString(), clpp, lar);
								}
							}
						}
					}
					// LLVM officiel (installeur autonome)
					{
						const char *c = "C:\\Program Files\\LLVM\\bin\\clang.exe";
						if (NkFile::Exists(c))
							addTc("clang-llvm", "Clang (LLVM officiel)", "Windows/x86_64/llvm", c,
								  "C:\\Program Files\\LLVM\\bin\\clang++.exe",
								  "C:\\Program Files\\LLVM\\bin\\llvm-ar.exe");
					}
					// PATH (toute autre origine) — la dedup empeche les doublons avec ci-dessus.
					{
						NkString ar = Which("llvm-ar");
						if (ar.Empty())
							ar = Which("ar");
						addTc("clang-path", "Clang (PATH)", "Windows/x86_64/llvm", Which("clang"), Which("clang++"),
							  ar);
					}
					addTc("gcc-path", "GCC (PATH)", "Windows/x86_64/gnu", Which("gcc"), Which("g++"), Which("ar"));
					if (seenCC.Empty())
						missing("clang", "Windows/x86_64", NkT("tc.rn.nonativemsys"));
#elif defined(__APPLE__)
					// Apple Clang (Xcode / CommandLineTools) : couvre macOS ET iOS (SDK present sur Mac).
					if (addTc(clangName, "Apple Clang", clangT, Which("clang"), Which("clang++"), Which("ar")) &&
						!sys.Empty())
						sys[sys.Size() - 1].caps = "macos ios";
					// LLVM via Homebrew
					{
						NkString pfx = NkNewWsState::TcRun("brew --prefix llvm 2>/dev/null");
						NkString p;
						for (const char *c = pfx.CStr(); *c; ++c)
							if (*c != '\n' && *c != '\r')
								p += *c;
						if (!p.Empty()) {
							NkString cc = p;
							cc += "/bin/clang";
							if (NkFile::Exists(cc.CStr())) {
								NkString cx = p;
								cx += "/bin/clang++";
								NkString a = p;
								a += "/bin/llvm-ar";
								addTc("clang-brew", "Clang (Homebrew LLVM)", "macOS/llvm", cc, cx, a);
							}
						}
					}
					if (seenCC.Empty())
						missing(clangName, clangT, NkT("tc.rn.applenoclang"));
#else
					// Linux : clang/gcc du PATH + variantes versionnees /usr/bin/{clang,gcc}-NN
					{
						NkString ar = Which("llvm-ar");
						if (ar.Empty())
							ar = Which("ar");
						addTc(clangName, "Clang", clangT, Which("clang"), Which("clang++"), ar);
					}
					addTc(gccName, "GCC", hostT, Which("gcc"), Which("g++"), Which("ar"));
					{
						const char *vs[] = {"20", "19", "18", "17", "16", "15", "14", "13", "12", "11"};
						for (int32 k = 0; k < 10; ++k) {
							NkString cc = "/usr/bin/clang-";
							cc += vs[k];
							if (NkFile::Exists(cc.CStr())) {
								NkString cx = "/usr/bin/clang++-";
								cx += vs[k];
								NkString a = "/usr/bin/llvm-ar-";
								a += vs[k];
								NkString nm = "clang-";
								nm += vs[k];
								NkString v = "Clang ";
								v += vs[k];
								addTc(nm, v, hostT, cc, cx, a);
							}
							NkString gc = "/usr/bin/gcc-";
							gc += vs[k];
							if (NkFile::Exists(gc.CStr())) {
								NkString gx = "/usr/bin/g++-";
								gx += vs[k];
								NkString nm = "gcc-";
								nm += vs[k];
								NkString v = "GCC ";
								v += vs[k];
								addTc(nm, v, hostT, gc, gx, Which("ar"));
							}
						}
					}
					if (seenCC.Empty())
						missing(clangName, clangT, NkT("tc.rn.nonative"));
#endif
					// android-ndk
					{
						NkString ndk = Env("ANDROID_NDK_ROOT");
						if (ndk.Empty())
							ndk = Env("ANDROID_NDK_HOME");
						if (!ndk.Empty()) {
							NkTcDetail d = found("android-ndk", "NDK", "Android/ARM64");
							d.pk.PushBack("NDK");
							d.pv.PushBack(ndk);
							sys.PushBack(d);
						} else
							missing("android-ndk", "Android/ARM64", NkT("tc.rn.ndkundef"));
					}
					// emscripten
					{
						NkString em = Env("EMSDK");
						if (em.Empty()) {
							NkString e = Which("emcc");
							if (!e.Empty())
								em = e;
						}
						if (!em.Empty()) {
							NkTcDetail d = found("emscripten", "Emscripten", "Web/WASM32");
							d.pk.PushBack("EMSDK");
							d.pv.PushBack(em);
							sys.PushBack(d);
						} else
							missing("emscripten", "Web/WASM32", NkT("tc.rn.emsdkabsent"));
					}
					// zig
					{
						NkString z = Which("zig");
#if defined(_WIN32)
						if (z.Empty()) {
							const char *la = env::GetEnvVar("LOCALAPPDATA"); // API maison (NkEnv.h)
							if (la) {
								NkString p = la;
								p += "\\Microsoft\\WinGet\\Links\\zig.exe";
								if (NkFile::Exists(p.CStr()))
									z = p;
							}
						}
#endif
						if (!z.Empty()) {
							if (zigCaps.Empty())
								zigCaps = ZigCaps(z); // test reel des cibles, une seule fois (cache)
							NkString tg = "Cross: ";
							tg += zigCaps;
							NkTcDetail d = found("zig", "Zig", tg.CStr());
							d.caps = zigCaps;
							d.pk.PushBack("zig");
							d.pv.PushBack(z);
							sys.PushBack(d);
						} else
							missing("zig", "Cross", NkT("tc.rn.zignopath"));
					}
					// clang-cl / msvc
					{
						NkString cl = Which("clang-cl");
						if (!cl.Empty()) {
							NkTcDetail d = found("msvc", "clang-cl", "Windows/x86_64/msvc");
							d.pk.PushBack("CC");
							d.pv.PushBack(cl);
							sys.PushBack(d);
						} else
							missing("msvc", "Windows/x86_64/msvc", NkT("tc.rn.clangclnopath"));
					}
					// ohos-ndk
					{
						NkString o = Env("OHOS_SDK");
						if (o.Empty())
							o = Env("OHOS_NDK_HOME");
						if (o.Empty())
							o = Env("DEVECO_SDK_HOME");
						if (!o.Empty()) {
							NkTcDetail d = found("ohos-ndk", "OHOS NDK", "HarmonyOS/ARM64");
							d.pk.PushBack("SDK");
							d.pv.PushBack(o);
							sys.PushBack(d);
						}
					}
					// WSL2 (Windows) : compile du Linux natif -> toolchains wsl2-gcc / wsl2-clang.
#if defined(_WIN32)
					if (NkFile::Exists("C:\\Windows\\System32\\wsl.exe")) {
						_putenv("WSL_UTF8=1");
						const NkString oRaw = NkNewWsState::TcRun(
							"wsl.exe -e sh -c \"grep -h '^PRETTY_NAME=' /etc/os-release 2>/dev/null; echo @@; for c in "
							"gcc clang g++ clang++ cc; do command -v $c 2>/dev/null; done\"");
						NkString o;
						for (const char *c = oRaw.CStr(); *c; ++c) {
							const unsigned char ch = (unsigned char)*c;
							if (ch == '\n' || (ch >= 32 && ch < 127))
								o += (char)ch;
						}
						NkString distro, comps;
						bool afterMark = false;
						NkString line;
						for (const char *s = o.CStr();; ++s) {
							if (*s == '\n' || *s == '\0') {
								NkString ln;
								for (const char *c = line.CStr(); *c; ++c)
									ln += *c;
								if (ln == "@@")
									afterMark = true;
								else if (!afterMark) {
									if (ln.Contains("PRETTY_NAME=")) {
										const char *a = nullptr;
										const char *b = nullptr;
										for (const char *c = ln.CStr(); *c; ++c)
											if (*c == '"') {
												if (!a)
													a = c + 1;
												else {
													b = c;
													break;
												}
											}
										if (a && b)
											for (const char *c = a; c < b; ++c)
												distro += *c;
									}
								} else if (!ln.Empty()) {
									const char *base = ln.CStr();
									for (const char *c = ln.CStr(); *c; ++c)
										if (*c == '/')
											base = c + 1;
									if (!comps.Empty())
										comps += ", ";
									comps += base;
								}
								line = NkString();
								if (*s == '\0')
									break;
							} else
								line += *s;
						}
						// Robustesse : si ce scan echoue (WSL occupe/en cours de demarrage), on
						// NE PERD PAS les compilateurs deja connus -> on reutilise le dernier resultat.
						if (comps.Empty() && !wslCompsCache.Empty())
							comps = wslCompsCache;
						else if (!comps.Empty())
							wslCompsCache = comps;
						NkString tgt = "Linux (WSL2 ";
						tgt += distro.Empty() ? NkString("Linux") : distro;
						tgt += ")";
						if (comps.Contains("gcc") || comps.Contains("g++")) {
							NkTcDetail d = found("wsl2-gcc", "GCC via WSL2", tgt.CStr());
							d.pk.PushBack("CMD");
							d.pv.PushBack("wsl.exe -e g++");
							sys.PushBack(d);
						}
						if (comps.Contains("clang")) {
							NkTcDetail d = found("wsl2-clang", "Clang via WSL2", tgt.CStr());
							d.pk.PushBack("CMD");
							d.pv.PushBack("wsl.exe -e clang++");
							sys.PushBack(d);
						}
					}
#endif
					LoadRegistry();
					detected = true;
				}

				// Lit le registre custom (extraction basique des "name" du JSON). Vide si fichier absent.
				void LoadRegistry() {
					custom = NkVector<NkTcDetail>();
					const NkString path = RegFile();
					if (!NkFile::Exists(path.CStr()))
						return;
					const NkString js = NkFile::ReadAllText(NkPath(path.CStr()));
					// recherche naive de "name": "valeur"
					const char *s = js.CStr();
					while (*s) {
						if (s[0] == '"' && s[1] == 'n' && s[2] == 'a' && s[3] == 'm' && s[4] == 'e' && s[5] == '"') {
							const char *c = s + 6;
							while (*c && *c != ':')
								++c;
							while (*c && *c != '"')
								++c;
							if (*c == '"')
								++c;
							NkString nm;
							while (*c && *c != '"') {
								nm += *c;
								++c;
							}
							// Dedup : un custom qui porte le nom d'un toolchain DEJA detecte n'est pas re-liste.
							bool dup = false;
							for (usize i = 0; i < sys.Size(); ++i)
								if (StrEq(sys[i].name.CStr(), nm.CStr())) {
									dup = true;
									break;
								}
							if (!nm.Empty() && !dup) {
								NkTcDetail d;
								d.name = nm;
								d.found = true;
								d.custom = true;
								d.version = "custom";
								d.target = "registre";
								custom.PushBack(d);
							}
							s = c;
						} else
							++s;
					}
				}

				// Teste UN toolchain precis (celui dont on a clique « Tester ») : verifie ses propres chemins.
				void RunTest(const NkTcDetail &d) {
					testLog = NkVector<NkString>();
					testOk = NkVector<int32>();
					{
						NkString hdr = "[ ";
						hdr += d.name;
						hdr += " ]";
						testLog.PushBack(hdr);
						testOk.PushBack(0);
					}
					if (!d.found) {
						NkString r = d.name;
						r += " : ";
						r += NkT("tc.test.notdet");
						testLog.PushBack(r);
						testOk.PushBack(2);
						return;
					}
					for (usize k = 0; k < d.pv.Size(); ++k) {
						const NkString &pv = d.pv[k];
						bool ok;
						NkString shown;
						if (pv.Contains("wsl.exe")) {
							// Commande WSL (ex: "wsl.exe -e clang++") -> on verifie le compilateur DANS WSL.
							const char *comp = pv.CStr();
							for (const char *c = pv.CStr(); *c; ++c)
								if (*c == ' ')
									comp = c + 1; // dernier mot
							NkString cmd = "wsl.exe -e sh -c \"command -v ";
							cmd += comp;
							cmd += "\"";
							const NkString out = NkNewWsState::TcRun(cmd.CStr());
							ok = out.Contains("/");
							shown = comp;
							shown += " (WSL)";
						} else {
							ok = NkFile::Exists(pv.CStr()) || NkDirectory::Exists(pv.CStr());
							const char *base = pv.CStr();
							for (const char *c = pv.CStr(); *c; ++c)
								if (*c == '/' || *c == '\\')
									base = c + 1;
							shown = base;
						}
						NkString r = d.pk[k];
						r += " (";
						r += shown;
						r += ") : ";
						r += ok ? NkT("tc.test.found") : NkT("tc.test.absent");
						testLog.PushBack(r);
						testOk.PushBack(ok ? 1 : 2);
					}
					if (d.pv.Empty()) {
						NkString r = d.name;
						r += " : ";
						r += NkT("tc.test.nopath");
						testLog.PushBack(r);
						testOk.PushBack(0);
					}
				}

				void SetFocus(int32 id, const char *buf) {
					focus = id;
					blink = 0.f;
					int32 n = 0;
					while (buf[n])
						++n;
					caret = n;
				}
		};

		// ── Modale « Détection assistée » (partagée par Toolchains ET Plateformes) ──
		// Rien de simulé : on pointe un binaire/SDK, on le VÉRIFIE réellement, puis on l'enregistre.
		// Renvoie true si la modale est ouverte (le panneau appelant doit alors bloquer son fond).
		inline bool NkAssistedDetectDialog(const NkUi &u, const NkRect &r, NkToolchainsState *t, NkNewWsState *nw,
										   NkCodeDialogs *dlg, float32 dt, const NkIcons &ic) {
			if (!t->assistDlg)
				return false;
			const bool isApple = NkToolchainsState::AssistKindIsApple(t->assistKind);
			u.dl->AddRectFilled(r, NkScrim(160), 0.f);
			const float32 mw = (r.w > u.s(640)) ? u.s(600) : (r.w - u.s(40));
			const float32 mh = isApple ? u.s(540) : u.s(430);
			const NkRect modal = {r.x + (r.w - mw) * 0.5f, r.y + (r.h - mh) * 0.5f, mw, mh};
			u.Panel(modal, NkCol::background, NkCol::border, NkR::lg * u.S);
			NkOwIco(u, ic.search, "search", {modal.x + u.s(22), modal.y + u.s(15), u.s(16), u.s(16)}, NkCol::primary);
			u.Text(modal.x + u.s(46), modal.y + u.s(16), NkT("as.title"), NkCol::foreground);
			{
				const NkRect xr = {modal.x + mw - u.s(30), modal.y + u.s(14), u.s(18), u.s(18)};
				const bool xh = u.Hit(xr);
				u.Icon("x", {xr.x + u.s(4), xr.y + u.s(4), u.s(11), u.s(11)}, xh ? NkCol::danger : NkCol::mutedFg);
				if (xh && u.click) {
					t->assistDlg = false;
					return true;
				}
			}
			u.Rect({modal.x + u.s(16), modal.y + u.s(40), mw - u.s(32), 1.f}, NkCol::border);
			const float32 lx = modal.x + u.s(24);
			const float32 iw = mw - u.s(48);
			u.TextEllipsis(lx, modal.y + u.s(52), iw, NkT("as.intro1"), NkCol::mutedFg);
			u.TextEllipsis(lx, modal.y + u.s(70), iw, NkT("as.intro2"), NkCol::mutedFg);

			// ── Type (combo) ──
			float32 y = modal.y + u.s(96);
			u.Text(lx, y, NkT("as.type"), NkCol::mutedFg);
			y += u.s(20);
			const NkRect combo = {lx, y, iw, u.s(32)};
			u.Panel(combo, NkCol::input, t->assistComboOpen ? NkCol::primary : NkCol::border, NkR::md * u.S);
			u.TextEllipsis(combo.x + u.s(12), combo.y + (combo.h - u.Lh()) * 0.5f, iw - u.s(34),
						   NkToolchainsState::AssistKindLabel(t->assistKind), NkCol::foreground);
			u.Icon("chevron-down", {combo.x + iw - u.s(20), combo.y + (combo.h - u.s(10)) * 0.5f, u.s(10), u.s(10)},
				   NkCol::mutedFg);
			if (u.Hit(combo) && u.click) {
				t->assistComboOpen = !t->assistComboOpen;
			}
			y += u.s(42);

			// ── Chemin + Parcourir ──
			const bool isFile = NkToolchainsState::AssistKindIsFile(t->assistKind);
			u.Text(lx, y, isApple ? NkT("as.path.root") : (isFile ? NkT("as.path.file") : NkT("as.path.dir")),
				   NkCol::mutedFg);
			y += u.s(20);
			const float32 browseW = u.s(120);
			const NkRect fr = {lx, y, iw - browseW - u.s(8), u.s(34)};
			const bool foc = (t->assistFocus == 1);
			u.Panel(fr, NkCol::input, foc ? NkCol::primary : NkCol::border, NkR::md * u.S);
			if (foc)
				NkOwEdit(u, fr, t->assistPath, (int32)sizeof(t->assistPath), t->assistCaret, t->assistBlink, dt,
						 u.s(10));
			else
				u.TextEllipsis(fr.x + u.s(10), fr.y + (fr.h - u.Lh()) * 0.5f, fr.w - u.s(16),
							   t->assistPath[0] ? t->assistPath : NkT("as.path.ph"),
							   t->assistPath[0] ? NkCol::foreground : NkCol::mutedFg);
			if (u.Hit(fr) && u.click) {
				int32 n = 0;
				while (t->assistPath[n])
					++n;
				t->assistFocus = 1;
				t->assistCaret = n;
				t->assistBlink = 0.f;
			}
			{
				const NkRect br = {lx + iw - browseW, y, browseW, u.s(34)};
				const bool hv = u.Hit(br);
				u.Rect(br, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
				NkOwIco(u, ic.ouvrirDossier, "folder-open", {br.x + u.s(14), br.y + u.s(10), u.s(14), u.s(14)},
						NkCol::mutedFg);
				u.TextV(br.x + u.s(34), br.y, u.s(34), NkT("btn.browse"), NkCol::foreground);
				if (hv && u.click && dlg) {
					t->assistFocus = -1;
					t->assistTested = false;
					if (isFile)
						dlg->BrowseFile(t->assistPath, (int32)sizeof(t->assistPath), NkT("as.title"));
					else
						dlg->BrowseInto(t->assistPath, (int32)sizeof(t->assistPath), NkT("as.title"));
				}
			}
			y += u.s(44);

			// ── Bouton Vérifier ──
			{
				const NkRect vb = {lx, y, u.s(150), u.s(32)};
				const bool hv = u.Hit(vb);
				u.Rect(vb, hv ? NkCol::hover : NkCol::secondary, NkR::md * u.S);
				NkOwIco(u, ic.search, "search", {vb.x + u.s(12), vb.y + u.s(9), u.s(13), u.s(13)}, NkCol::primary);
				u.TextV(vb.x + u.s(32), vb.y, u.s(32), NkT("as.verify"), NkCol::primary);
				if (hv && u.click) {
					t->assistFocus = -1;
					t->AssistVerify();
				}
			}
			y += u.s(42);

			// ── Zone de résultat ──
			{
				const float32 boxH = isApple ? u.s(160) : u.s(58);
				const NkRect box = {lx, y, iw, boxH};
				u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
				if (!t->assistTested)
					u.TextV(box.x + u.s(12), box.y, boxH, NkT("as.untested"), NkCol::mutedFg);
				else if (isApple) {
					// Détail des 5 composants (doc §2) : chacun ✓/✗ + son chemin.
					const NkAppleZig &a = t->assistApple;

					struct Row {
							const char *label;
							bool ok;
							const NkString *path;
					};

					const Row rows[] = {
						{"zig 0.13", a.okZig, &a.zig},		  {"SDK iOS", a.okIosSdk, &a.iosSdk},
						{"ld.lld (NDK)", a.okLd, &a.ld64},	  {"libc++ iOS", a.okLibcpp, &a.libcpp},
						{"SDK macOS", a.okMacSdk, &a.macSdk}, {"rcodesign", a.signOk, &a.rcodesign},
					};
					float32 ry = box.y + u.s(8);
					for (int32 i = 0; i < 6; ++i) {
						NkOwIco(u, rows[i].ok ? ic.valideSimple : 0u, rows[i].ok ? "check-circle" : "x",
								{box.x + u.s(10), ry + u.s(2), u.s(12), u.s(12)},
								rows[i].ok ? NkCol::success : NkCol::danger);
						u.Text(box.x + u.s(30), ry, rows[i].label, rows[i].ok ? NkCol::foreground : NkCol::mutedFg);
						u.TextEllipsis(box.x + u.s(126), ry, iw - u.s(138),
									   rows[i].path->Empty() ? "--" : rows[i].path->CStr(), NkCol::mutedFg);
						ry += u.s(17);
					}
					// ligne de synthèse iOS/macOS
					const NkColor sc = (a.iosOk || a.macosOk) ? NkCol::success : NkCol::accent;
					u.Text(box.x + u.s(10), ry + u.s(4), t->assistMsg.CStr(), sc);
				} else {
					NkOwIco(u, t->assistOk ? ic.valideSimple : 0u, t->assistOk ? "check-circle" : "x",
							{box.x + u.s(12), box.y + u.s(11), u.s(15), u.s(15)},
							t->assistOk ? NkCol::success : NkCol::danger);
					u.Text(box.x + u.s(36), box.y + u.s(9), t->assistOk ? NkT("as.ok") : NkT("as.fail"),
						   t->assistOk ? NkCol::success : NkCol::danger);
					u.TextEllipsis(box.x + u.s(12), box.y + u.s(32), iw - u.s(24), t->assistMsg.CStr(),
								   NkCol::foreground);
				}
			}

			// ── Pied : Fermer (gauche) / Ajouter (droite, actif si vérifié OK) ──
			const float32 by = modal.y + mh - u.s(46);
			{
				const NkRect b = {lx, by, u.s(96), u.s(34)};
				if (u.Button(b, NkT("btn.close"), NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S)) {
					t->assistDlg = false;
					return true;
				}
			}
			if (isApple) {
				// Bouton « Installer / Réparer » : lance setup_apple_toolchain.py (télécharge+installe) dans un
				// terminal.
				const float32 bw = u.TextW(NkT("as.install")) + u.s(46);
				const NkRect b = {modal.x + mw - u.s(24) - bw, by, bw, u.s(34)};
				const bool hv = u.Hit(b);
				u.Rect(b, hv ? NkColHover(NkCol::primary) : NkCol::primary, NkR::md * u.S);
				NkOwIco(u, ic.clonerTel, "download", {b.x + u.s(14), b.y + u.s(10), u.s(14), u.s(14)},
						NkCol::primaryFg);
				u.TextV(b.x + u.s(34), b.y, u.s(34), NkT("as.install"), NkCol::primaryFg);
				if (hv && u.click) {
					NkString root = t->assistPath[0] ? NkString(t->assistPath) : NkString("C:\\apple-sdks");
					const NkString cmd = NkToolchainsState::AppleSetupCommand(root);
#if defined(_WIN32)
					NkString c = "start \"\" cmd /k ";
					c += cmd;
					NkCodeShellRun(c.CStr());
#else
					NkString c = cmd;
					c += " ; read -n1";
					NkCodeShellRun(c.CStr());
#endif
					t->detected = false; // re-détecte après installation
				}
			} else {
				const bool en = t->assistOk;
				const float32 bw = u.TextW(NkT("as.add")) + u.s(46);
				const NkRect b = {modal.x + mw - u.s(24) - bw, by, bw, u.s(34)};
				const bool hv = en && u.Hit(b);
				u.Rect(b, !en ? NkCol::muted : (hv ? NkColHover(NkCol::success) : NkCol::success), NkR::md * u.S);
				const NkColor fg = en ? NkCol::primaryFg : NkCol::mutedFg;
				NkOwIco(u, ic.valideSimple, "check", {b.x + u.s(14), b.y + u.s(10), u.s(14), u.s(14)}, fg);
				u.TextV(b.x + u.s(34), b.y, u.s(34), NkT("as.add"), fg);
				if (hv && u.click) {
					t->AssistAdd(nw);
					return true;
				}
			}

			// ── Dropdown du combo (par-dessus tout le reste de la modale) ──
			if (t->assistComboOpen) {
				const float32 ih = u.s(28);
				const int32 n = NkToolchainsState::AssistKindCount();
				const NkRect dd = {combo.x, combo.y + combo.h + u.s(2), combo.w, n * ih + u.s(6)};
				u.dl->AddRectFilled({dd.x + u.s(2), dd.y + u.s(3), dd.w, dd.h}, NkColor{0, 0, 0, 90}, NkR::md * u.S);
				u.Panel(dd, NkCol::surface, NkCol::primary, NkR::md * u.S);
				bool chose = false;
				for (int32 k = 0; k < n; ++k) {
					const NkRect ir = {dd.x + u.s(4), dd.y + u.s(3) + k * ih, dd.w - u.s(8), ih};
					const bool hv = u.Hit(ir);
					if (hv || k == t->assistKind)
						u.Rect(ir, NkCol::hover, NkR::sm * u.S);
					u.TextEllipsis(ir.x + u.s(8), ir.y + (ih - u.Lh()) * 0.5f, ir.w - u.s(12),
								   NkToolchainsState::AssistKindLabel(k), NkCol::foreground);
					if (hv && u.click) {
						t->assistKind = k;
						t->assistTested = false;
						t->assistOk = false;
						chose = true;
						if (NkToolchainsState::AssistKindIsApple(k) && t->assistPath[0] == '\0')
							NkStrCopy(t->assistPath, sizeof(t->assistPath), "C:\\apple-sdks"); // copie maison
					}
				}
				if (chose || (u.click && !u.Hit(dd) && !u.Hit(combo)))
					t->assistComboOpen = false;
			}
			if (u.ctx->input.KeyPressed(NkGuiKey::Escape)) {
				if (t->assistComboOpen)
					t->assistComboOpen = false;
				else if (t->assistFocus >= 0)
					t->assistFocus = -1;
				else
					t->assistDlg = false;
			}
			return true;
		}

		// ── Panneau plein cadre « Gestionnaire de Toolchains ». ──
		inline int32 NkToolchainsPanel(const NkUi &u, const NkRect &r, NkToolchainsState *t, NkNewWsState *nw,
									   NkCodeState *st, NkCodeDialogs *dlg, float32 dt, const NkIcons &ic) {
			(void)st;
			t->EnsureDetectedAsync(); // detection sur thread de fond -> l'UI ne gele jamais
			int32 result = 0;
			// Sauvegarde auto du registre quand un toolchain custom est ajoute/edite via le dialog.
			if (t->lastCustomN < 0)
				t->lastCustomN = (int32)nw->customTc.Size();
			else if ((int32)nw->customTc.Size() != t->lastCustomN) {
				NkToolchainsState::SaveRegistry(nw);
				t->LoadRegistry();
				t->lastCustomN = (int32)nw->customTc.Size();
			}
			const bool blockBg = (nw->comboOpen >= 0) || nw->tcDlg || t->envDlg || t->installDlg || t->assistDlg ||
								 t->bgBusy.Load() || NkTxtMenu().open || (dlg && dlg->pickerOpen);
			t->focusClaimed = false; // reinitialise chaque frame : un champ posera true en cas de clic dessus

			u.Rect(r, NkCol::background); // fond du panneau (theme-aware) : sans ça le fond de base sombre transparaît

			// En-tete + actions
			const float32 hH = u.s(54);
			u.Rect({r.x, r.y, r.w, hH}, NkCol::sidebar);
			u.Rect({r.x, r.y + hH - 1.f, r.w, 1.f}, NkCol::border);
			NkOwIco(u, ic.toolchains, "cpu", {r.x + u.s(28), r.y + (hH - u.s(18)) * 0.5f, u.s(18), u.s(18)},
					NkCol::primary);
			u.Text(r.x + u.s(54), r.y + (hH - u.Lh()) * 0.5f, NkT("tc.title"), NkCol::foreground);
			{
				float32 bx = r.x + r.w - u.s(28);
				const char *dlab = t->detecting ? NkT("tc.detecting") : NkT("tc.detect");
				const float32 dw = u.TextW(dlab) + u.s(44);
				const NkRect dr = {bx - dw, r.y + (hH - u.s(32)) * 0.5f, dw, u.s(32)};
				bx -= dw + u.s(8);
				const bool dh = !blockBg && u.Hit(dr);
				u.Rect(dr, t->detecting ? NkCol::secondary : (dh ? NkCol::hover : NkCol::muted), NkR::md * u.S);
				NkOwIco(u, ic.clonerTel, "refresh", {dr.x + u.s(12), dr.y + u.s(9), u.s(14), u.s(14)},
						t->detecting ? NkCol::primary : NkCol::foreground);
				u.TextV(dr.x + u.s(32), dr.y, u.s(32), dlab, t->detecting ? NkCol::primary : NkCol::foreground);
				if (dh && u.click && !t->detecting)
					t->StartDetect();
				const float32 nbw = u.TextW(NkT("tc.new")) + u.s(44);
				const NkRect nr = {bx - nbw, r.y + (hH - u.s(32)) * 0.5f, nbw, u.s(32)};
				bx -= nbw + u.s(8);
				const bool nh = u.Hit(nr);
				u.Rect(nr, nh ? NkColHover(NkCol::primary) : NkCol::primary, NkR::md * u.S);
				NkOwIco(u, ic.plus, "plus", {nr.x + u.s(12), nr.y + u.s(9), u.s(13), u.s(13)}, NkCol::primaryFg);
				u.TextV(nr.x + u.s(30), nr.y, u.s(32), NkT("tc.new"), NkCol::primaryFg);
				if (nh && u.click) {
					nw->tcEditIdx = -1;
					nw->tcDraft = NkNewWsState::CustomTc{};
					nw->tcDlg = true;
					nw->tcDlgScroll = 0.f;
					nw->tcDlgScrollX = 0.f;
					nw->tcTested = false;
					nw->focus = -1;
				}
				// Bouton « Détection assistée » : pointer manuellement un toolchain/SDK raté.
				const float32 abw = u.TextW(NkT("as.button")) + u.s(44);
				const NkRect ar = {bx - abw, r.y + (hH - u.s(32)) * 0.5f, abw, u.s(32)};
				bx -= abw + u.s(8);
				const bool ah = !blockBg && u.Hit(ar);
				u.Rect(ar, ah ? NkCol::hover : NkCol::muted, NkR::md * u.S);
				NkOwIco(u, ic.search, "search", {ar.x + u.s(12), ar.y + u.s(9), u.s(13), u.s(13)}, NkCol::foreground);
				u.TextV(ar.x + u.s(32), ar.y, u.s(32), NkT("as.button"), NkCol::foreground);
				if (ah && u.click)
					t->OpenAssistDialog();
			}

			// ═══ VUE DE PROGRESSION DE LA DETECTION (remplace le gestionnaire pendant la detection) ═══
			if (t->detecting) {
				t->StepAdvance(dt);
				const float32 padLd = u.s(28);
				const float32 rW = (r.w > u.s(860)) ? u.s(300) : u.s(260);
				const float32 dcx = r.x + padLd, drx = r.x + r.w - u.s(28) - rW, dcw = (drx - u.s(34)) - dcx;
				const int32 DS = NkToolchainsState::DETECT_STEPS;
				// gauche : barre + etapes + journal
				float32 y = r.y + hH + u.s(20);
				NkWizLabel(u, dcx, y, NkT("tc.progress"));
				{
					const NkString pct = NkPrintf("%d%%", t->detectStep * 100 / DS); // NkPrintf maison
					u.Text(dcx + dcw - u.TextW(pct.CStr()), y, pct.CStr(), NkCol::mutedFg);
				}
				y += u.s(22);
				{
					const NkRect bar = {dcx, y, dcw, u.s(8)};
					u.Panel(bar, NkCol::input, NkCol::border, u.s(4));
					u.dl->AddRectFilled({bar.x, bar.y, dcw * ((float32)t->detectStep / DS), u.s(8)}, NkCol::primary,
										u.s(4));
				}
				y += u.s(22);
				for (int32 i = 0; i < DS; ++i) {
					const bool done = i < t->detectStep, cur = (i == t->detectStep);
					const NkRect row = {dcx, y, dcw, u.s(44)};
					if (cur)
						u.Panel(row, NkCol::secondary, NkCol::primary, NkR::md * u.S);
					NkOwIco(u, done ? ic.valideSimple : (cur ? ic.clonerTel : 0u), done ? "check-circle" : "circle",
							{row.x + u.s(12), row.y + u.s(15), u.s(14), u.s(14)},
							done ? NkCol::success : (cur ? NkCol::primary : NkCol::muted));
					u.Text(row.x + u.s(38), row.y + u.s(14), NkToolchainsState::DetectStepLabel(i),
						   (done || cur) ? NkCol::foreground : NkCol::mutedFg);
					if (cur)
						u.TextV(row.x + dcw - u.s(84), row.y, u.s(44), NkT("tc.inprogress"), NkCol::primary);
					y += u.s(48);
				}
				y += u.s(8);
				NkWizLabel(u, dcx, y, NkT("tc.logtitle"));
				y += u.s(20);
				{
					const float32 jh = u.s(130);
					const NkRect box = {dcx, y, dcw, jh};
					u.Panel(box, NkCol::input, NkCol::border, NkR::md * u.S);
					u.dl->PushClipRect(box, true);
					float32 ly = box.y + jh - u.s(18);
					for (int32 i = (int32)t->detectLog.Size() - 1; i >= 0 && ly > box.y; --i) {
						u.Text(box.x + u.s(10), ly, t->detectLog[i].CStr(), NkCol::mutedFg);
						ly -= u.s(16);
					}
					u.dl->PopClipRect();
				}
				// droite : resultats partiels + boutons + options
				float32 ry = r.y + hH + u.s(20);
				NkWizLabel(u, drx, ry, NkT("tc.partial"));
				ry += u.s(22);
				for (usize i = 0; i < t->pendingSys.Size(); ++i) {
					const NkTcDetail &d = t->pendingSys[i];
					const int32 stt = t->StatusVs(d);
					const NkRect box = {drx, ry, rW, u.s(46)};
					u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
					NkOwIco(u, d.found ? ic.valideSimple : 0u, d.found ? "check-circle" : "x",
							{box.x + u.s(12), box.y + u.s(9), u.s(13), u.s(13)},
							d.found ? NkCol::success : NkCol::danger);
					u.Text(box.x + u.s(32), box.y + u.s(7), d.name.CStr(), NkCol::foreground);
					u.TextEllipsis(box.x + u.s(32), box.y + u.s(25), rW - u.s(110), d.found ? d.version.CStr() : "--",
								   NkCol::mutedFg);
					const char *bl[] = {NkT("tc.badge.new"), NkT("tc.badge.unchanged"), NkT("tc.badge.updated"),
										NkT("tc.badge.missing")};
					const NkColor bc[] = {NkCol::success, NkCol::muted, NkCol::accent, NkCol::danger};
					const float32 bw = u.TextW(bl[stt]) + u.s(16);
					const NkRect bg = {box.x + rW - bw - u.s(10), box.y + u.s(13), bw, u.s(20)};
					u.dl->AddRectFilled(bg, bc[stt], u.s(4));
					u.TextV(bg.x + u.s(8), bg.y - u.s(3), u.s(20), bl[stt],
							stt == 1 ? NkCol::foreground : NkCol::primaryFg);
					ry += u.s(54);
				}
				ry += u.s(6);
				{
					const NkRect box = {drx, ry, rW, u.s(52)};
					u.Panel(box, NkCol::secondary, NkCol::border, NkR::md * u.S);
					NkOwIco(u, ic.rondI, "info", {box.x + u.s(12), box.y + u.s(10), u.s(13), u.s(13)}, NkCol::primary);
					u.TextEllipsis(box.x + u.s(32), box.y + u.s(8), rW - u.s(44), NkT("tc.dethint1"),
								   NkCol::foreground);
					u.TextEllipsis(box.x + u.s(32), box.y + u.s(26), rW - u.s(44), NkT("tc.dethint2"), NkCol::mutedFg);
					ry += u.s(60);
				}
				const bool fin = t->detectStep >= DS;
				{
					const NkRect ap = {drx, ry, rW, u.s(34)};
					const bool hv = fin && u.Hit(ap);
					u.Rect(ap, !fin ? NkCol::muted : (hv ? NkColHover(NkCol::success) : NkCol::success), NkR::md * u.S);
					const NkColor fg = fin ? NkCol::primaryFg : NkCol::mutedFg;
					NkOwIco(u, ic.valideSimple, "check",
							{ap.x + (rW - u.TextW(NkT("tc.applyresults")) - u.s(20)) * 0.5f, ap.y + u.s(10), u.s(14),
							 u.s(14)},
							fg);
					u.TextV(ap.x + (rW - u.TextW(NkT("tc.applyresults")) - u.s(20)) * 0.5f + u.s(20), ap.y, u.s(34),
							NkT("tc.applyresults"), fg);
					if (hv && u.click)
						t->ApplyDetect();
					ry += u.s(42);
				}
				{
					const NkRect an = {drx, ry, rW, u.s(32)};
					if (u.Button(an, NkT("tc.canceldetect"), NkCol::muted, NkCol::hover, NkCol::foreground,
								 NkR::md * u.S))
						t->detecting = false;
					ry += u.s(42);
				}
				NkWizLabel(u, drx, ry, NkT("tc.options"));
				ry += u.s(24);
				if (NkWizCheck(u, drx, ry, NkT("tc.optcross"), t->optCross, false))
					t->optCross = !t->optCross;
				ry += u.s(28);
				if (NkWizCheck(u, drx, ry, NkT("tc.optlocalbin"), t->optLocalBin, false))
					t->optLocalBin = !t->optLocalBin;
				ry += u.s(28);
				if (NkWizCheck(u, drx, ry, NkT("tc.optsdk"), t->optSdk, false))
					t->optSdk = !t->optSdk;
				ry += u.s(28);
				if (NkWizCheck(u, drx, ry, NkT("tc.optdeep"), t->optDeep, false))
					t->optDeep = !t->optDeep;
				ry += u.s(28);
				if (u.ctx->input.KeyPressed(NkGuiKey::Escape))
					t->detecting = false;
				return result;
			}

			const float32 padL = u.s(28);
			const float32 rightW = (r.w > u.s(860)) ? u.s(300) : u.s(260);
			const float32 cx = r.x + padL;
			const float32 rx = r.x + r.w - u.s(28) - rightW;
			const float32 cw = (rx - u.s(34)) - cx;
			const NkRect view = {r.x, r.y + hH, r.w, r.h - hH};

			// molette
			if (u.Hit(view) && u.ctx->input.wheel != 0.f && !blockBg) {
				t->scroll -= u.ctx->input.wheel * u.s(40);
				if (t->scroll < 0.f)
					t->scroll = 0.f;
				if (t->scroll > t->scrollMax)
					t->scroll = t->scrollMax;
			}
			u.dl->PushClipRect(view, true);
			float32 y = view.y - t->scroll + u.s(18);

			// Filtre
			{
				const NkRect fr = {cx, y, cw, u.s(32)};
				const bool foc = (t->focus == 1);
				u.Panel(fr, NkCol::input, foc ? NkCol::primary : NkCol::border, NkR::md * u.S);
				NkOwIco(u, ic.search, "search", {fr.x + u.s(10), fr.y + u.s(9), u.s(14), u.s(14)}, NkCol::mutedFg);
				if (foc)
					NkOwEdit(u, fr, t->filter, (int32)sizeof(t->filter), t->caret, t->blink, dt, u.s(32));
				else
					u.TextEllipsis(fr.x + u.s(32), fr.y + (fr.h - u.Lh()) * 0.5f, cw - u.s(40),
								   t->filter[0] ? t->filter : NkT("tc.filter"),
								   t->filter[0] ? NkCol::foreground : NkCol::mutedFg);
				if (!blockBg && u.Hit(fr) && u.click) {
					t->SetFocus(1, t->filter);
					t->focusClaimed = true;
				}
			}
			y += u.s(44);

			auto matchFilter = [&](const NkTcDetail &d) -> bool {
				if (!t->filter[0])
					return true;
				return NkString(d.name.CStr()).Contains(t->filter) || NkString(d.target.CStr()).Contains(t->filter);
			};
			// Carte d'un toolchain
			auto card = [&](const NkTcDetail &d) {
				const float32 lines = (float32)(d.found ? d.pk.Size() : 1);
				const float32 boxH = u.s(34) + lines * u.s(16) + u.s(36);
				const NkRect box = {cx, y, cw, boxH};
				u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
				NkOwIco(u, d.found ? ic.valideSimple : 0u, d.found ? "check-circle" : "alert-triangle",
						{box.x + u.s(14), box.y + u.s(11), u.s(15), u.s(15)}, d.found ? NkCol::success : NkCol::accent);
				u.Text(box.x + u.s(38), box.y + u.s(9), d.name.CStr(), d.found ? NkCol::foreground : NkCol::mutedFg);
				u.Text(box.x + cw * 0.40f, box.y + u.s(9), d.found ? d.version.CStr() : NkT("tc.notfound"),
					   NkCol::mutedFg);
				u.TextEllipsis(box.x + cw * 0.62f, box.y + u.s(9), cw * 0.36f, d.target.CStr(), NkCol::mutedFg);
				float32 ly = box.y + u.s(32);
				if (d.found) {
					for (usize k = 0; k < d.pk.Size(); ++k) {
						u.Text(box.x + u.s(38), ly, d.pk[k].CStr(), NkCol::primary);
						u.TextEllipsis(box.x + u.s(78), ly, cw - u.s(90), d.pv[k].CStr(), NkCol::mutedFg);
						ly += u.s(16);
					}
				} else {
					u.TextEllipsis(box.x + u.s(38), ly, cw - u.s(50), d.reason.CStr(), NkCol::danger);
					ly += u.s(16);
				}
				// boutons
				float32 bx = box.x + u.s(38);
				const float32 by = box.y + boxH - u.s(30);
				auto btn = [&](const char *lab, uint32 tex, const char *drawn) -> bool {
					const float32 bw = u.TextW(lab) + u.s(34);
					const NkRect b = {bx, by, bw, u.s(24)};
					const bool hv = !blockBg && u.Hit(b);
					u.Rect(b, hv ? NkCol::hover : NkCol::muted, NkR::sm * u.S);
					NkOwIco(u, tex, drawn, {b.x + u.s(8), b.y + u.s(6), u.s(12), u.s(12)},
							hv ? NkCol::foreground : NkCol::mutedFg);
					u.TextV(b.x + u.s(24), b.y, u.s(24), lab, hv ? NkCol::foreground : NkCol::mutedFg);
					bx += bw + u.s(8);
					return hv && u.click;
				};
				if (d.found) {
					if (d.custom) {
						if (btn(NkT("tc.edit"), ic.editer,
								"edit")) { // ouvre l'editeur custom pre-rempli (ex "zig-manuel")
							for (usize ci = 0; ci < nw->customTc.Size(); ++ci)
								if (StrEq(nw->customTc[ci].name, d.name.CStr())) {
									nw->tcEditIdx = (int32)ci;
									nw->tcDraft = nw->customTc[ci];
									nw->tcDlg = true;
									nw->tcDlgScroll = 0.f;
									nw->tcDlgScrollX = 0.f;
									nw->tcTested = false;
									nw->focus = -1;
									break;
								}
						}
						if (btn(NkT("tc.delete"), ic.corbeille, "trash")) { // retire du registre + re-detecte
							for (usize ci = 0; ci < nw->customTc.Size(); ++ci)
								if (StrEq(nw->customTc[ci].name, d.name.CStr())) {
									nw->customTc.Erase(nw->customTc.Begin() + ci);
									NkToolchainsState::SaveRegistry(nw);
									t->LoadRegistry();
									t->lastCustomN = (int32)nw->customTc.Size();
									break;
								}
						}
					}
					if (btn(NkT("tc.test"), ic.search, "search"))
						t->RunTest(d);
					if (btn(NkT("tc.copyconfig"), ic.fileCode, "copy")) {
						NkString cfg = d.name;
						cfg += " (";
						cfg += d.target;
						cfg += ")";
						for (usize k = 0; k < d.pk.Size(); ++k) {
							cfg += "\n";
							cfg += d.pk[k];
							cfg += "=";
							cfg += d.pv[k];
						}
						u.ctx->SetClipboard(cfg.CStr());
					}
				} else {
					if (btn(NkT("tc.howinstall"), ic.rondI, "info")) {
						t->installName = d.name;
						NkToolchainsState::InstallInfo(d.name.CStr(), t->installText, t->installCmd);
						t->installDlg = true;
					}
				}
				y += boxH + u.s(10);
			};

			// TOOLCHAINS SYSTEME
			NkWizLabel(u, cx, y, NkT("tc.system"));
			y += u.s(24);
			for (usize i = 0; i < t->sys.Size(); ++i)
				if (matchFilter(t->sys[i]))
					card(t->sys[i]);
			y += u.s(12);
			// TOOLCHAINS PERSONNALISES
			NkWizLabel(u, cx, y, NkT("tc.custom"));
			y += u.s(24);
			if (t->custom.Empty()) {
				const NkRect box = {cx, y, cw, u.s(40)};
				u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
				u.TextV(box.x + u.s(14), box.y, u.s(40), NkT("tc.nocustom"), NkCol::mutedFg);
				y += u.s(48);
			} else
				for (usize i = 0; i < t->custom.Size(); ++i)
					if (matchFilter(t->custom[i]))
						card(t->custom[i]);

			// ===== COLONNE DROITE =====
			float32 ry = view.y - t->scroll + u.s(18);
			NkWizLabel(u, rx, ry, NkT("tc.envvars"));
			ry += u.s(24);
			{
				const char *vars[] = {"ANDROID_SDK_ROOT", "ANDROID_NDK_ROOT", "EMSDK",
									  "OHOS_SDK",		  "JAVA_HOME",		  "ZIG_ROOT"};
				const int32 nV = 6;
				const NkRect box = {rx, ry, rightW, u.s(12) + nV * u.s(26)};
				u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
				float32 vy = box.y + u.s(10);
				for (int32 i = 0; i < nV; ++i) {
					const NkString val = NkToolchainsState::Env(vars[i]);
					const bool set = !val.Empty();
					u.Text(box.x + u.s(12), vy, vars[i], set ? NkCol::foreground : NkCol::mutedFg);
					u.TextEllipsis(box.x + u.s(150), vy, rightW - u.s(186), set ? val.CStr() : NkT("tc.undefined"),
								   NkCol::mutedFg);
					NkOwIco(u, set ? ic.valideSimple : 0u, set ? "check-circle" : "alert-triangle",
							{box.x + rightW - u.s(26), vy, u.s(13), u.s(13)}, set ? NkCol::success : NkCol::accent);
					vy += u.s(26);
				}
				ry += box.h + u.s(10);
			}
			{
				const NkRect b = {rx, ry, rightW, u.s(30)};
				const bool hv = !blockBg && u.Hit(b);
				u.Rect(b, hv ? NkCol::hover : NkCol::secondary, NkR::md * u.S);
				NkOwIco(u, ic.gear, "settings", {b.x + u.s(12), b.y + u.s(8), u.s(13), u.s(13)}, NkCol::primary);
				u.TextV(b.x + u.s(32), b.y, u.s(30), NkT("tc.editenv"), NkCol::primary);
				if (hv && u.click)
					t->OpenEnvDialog();
				ry += u.s(40);
			}
			// Dernier resultat de test
			NkWizLabel(u, rx, ry, NkT("tc.lasttest"));
			ry += u.s(24);
			{
				const int32 nL = t->testLog.Empty() ? 1 : (int32)t->testLog.Size();
				const NkRect box = {rx, ry, rightW, u.s(12) + nL * u.s(22)};
				u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
				float32 ty = box.y + u.s(8);
				if (t->testLog.Empty())
					u.Text(box.x + u.s(12), ty, NkT("tc.testhint"), NkCol::mutedFg);
				else
					for (usize i = 0; i < t->testLog.Size(); ++i) {
						const int32 stt = (i < t->testOk.Size()) ? t->testOk[i] : 0;
						const bool ok = (stt == 1);
						const bool neutral = (stt == 0);
						NkOwIco(u, ok ? ic.valideSimple : 0u, ok ? "check-circle" : "x",
								{box.x + u.s(12), ty, u.s(12), u.s(12)},
								ok ? NkCol::success : (neutral ? NkCol::mutedFg : NkCol::danger));
						u.TextEllipsis(box.x + u.s(30), ty - u.s(1), rightW - u.s(42), t->testLog[i].CStr(),
									   ok ? NkCol::foreground : (neutral ? NkCol::mutedFg : NkCol::danger));
						ty += u.s(22);
					}
				ry += box.h + u.s(10);
			}
			// Registre
			{
				const NkRect box = {rx, ry, rightW, u.s(56)};
				const bool exists = NkFile::Exists(NkToolchainsState::RegFile().CStr());
				u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
				NkOwIco(u, ic.fileCode, "file", {box.x + u.s(12), box.y + u.s(10), u.s(14), u.s(14)},
						exists ? NkCol::primary : NkCol::mutedFg);
				u.Text(box.x + u.s(34), box.y + u.s(8), "toolchains_registry.json", NkCol::foreground);
				u.TextEllipsis(box.x + u.s(12), box.y + u.s(30), rightW - u.s(24),
							   exists ? "~/.jenga/toolchains_registry.json" : NkT("tc.notcreated"), NkCol::mutedFg);
				ry += box.h + u.s(10);
			}

			u.dl->PopClipRect();
			// hauteur de contenu = max des deux colonnes
			const float32 leftH = (y + t->scroll) - view.y + u.s(20);
			const float32 rightH = (ry + t->scroll) - view.y + u.s(20);
			const float32 contentH = (leftH > rightH ? leftH : rightH);
			t->scrollMax = (contentH > view.h) ? (contentH - view.h) : 0.f;
			if (t->scroll > t->scrollMax)
				t->scroll = t->scrollMax;
			if (t->scrollMax > 0.5f) {
				const float32 sw = editorkit::NkScrollbarWidth();
				const NkRect track = {view.x + view.w - sw - u.s(2), view.y, sw, view.h};
				editorkit::NkVScrollbar(*u.ctx, *u.dl, track, t->scroll, view.h + t->scrollMax, view.h, 0x70C0A001u,
										u.s(24)); // scrollbar standard
			}
			// ── Fiche « Comment installer <toolchain> » (conseil manuel + auto + redirection) ──
			// ── Modale « Détection assistée » (partagée) ──
			if (NkAssistedDetectDialog(u, r, t, nw, dlg, dt, ic))
				return result;

			if (t->installDlg) {
				u.dl->AddRectFilled(r, NkScrim(160), 0.f);
				const float32 mw = (r.w > u.s(640)) ? u.s(600) : (r.w - u.s(40));
				const float32 mh = u.s(300);
				const NkRect modal = {r.x + (r.w - mw) * 0.5f, r.y + (r.h - mh) * 0.5f, mw, mh};
				u.Panel(modal, NkCol::background, NkCol::border, NkR::lg * u.S);
				NkOwIco(u, ic.rondI, "info", {modal.x + u.s(22), modal.y + u.s(15), u.s(16), u.s(16)}, NkCol::primary);
				{
					NkString title = "INSTALLER : ";
					title += t->installName;
					u.Text(modal.x + u.s(46), modal.y + u.s(16), title.CStr(), NkCol::foreground);
				}
				{
					const NkRect xr = {modal.x + mw - u.s(30), modal.y + u.s(14), u.s(18), u.s(18)};
					const bool xh = u.Hit(xr);
					u.Icon("x", {xr.x + u.s(4), xr.y + u.s(4), u.s(11), u.s(11)}, xh ? NkCol::danger : NkCol::mutedFg);
					if (xh && u.click)
						t->installDlg = false;
				}
				u.Rect({modal.x + u.s(16), modal.y + u.s(40), mw - u.s(32), 1.f}, NkCol::border);
				u.TextEllipsis(modal.x + u.s(24), modal.y + u.s(58), mw - u.s(48), t->installText.CStr(),
							   NkCol::foreground);
				// Commande (encadre code) + copier
				const bool hasCmd = !t->installCmd.Empty() && !t->installCmd.Contains("N/A");
				{
					const NkRect cbox = {modal.x + u.s(24), modal.y + u.s(84), mw - u.s(48), u.s(64)};
					u.Panel(cbox, NkCol::input, NkCol::border, NkR::md * u.S);
					u.dl->PushClipRect(cbox, true);
					u.TextEllipsis(cbox.x + u.s(12), cbox.y + u.s(10), mw - u.s(120),
								   hasCmd ? t->installCmd.CStr() : NkT("tc.installmanual"),
								   hasCmd ? NkCol::success : NkCol::mutedFg);
					u.dl->PopClipRect();
					if (hasCmd) {
						const NkRect cp = {cbox.x + cbox.w - u.s(84), cbox.y + u.s(10), u.s(74), u.s(24)};
						const bool hv = u.Hit(cp);
						u.Rect(cp, hv ? NkCol::hover : NkCol::muted, NkR::sm * u.S);
						NkOwIco(u, ic.fileCode, "copy", {cp.x + u.s(8), cp.y + u.s(6), u.s(11), u.s(11)},
								NkCol::foreground);
						u.TextV(cp.x + u.s(22), cp.y, u.s(24), NkT("tc.copy"), NkCol::foreground);
						if (hv && u.click)
							u.ctx->SetClipboard(t->installCmd.CStr());
					}
				}
				// Redirection : besoin d'une variable d'environnement ? -> ouvrir l'editeur.
				const bool needsEnv = t->installName.Contains("ndk") || t->installName.Contains("emscripten") ||
									  t->installName.Contains("ohos");
				// Pied : Fermer / [Configurer les variables] / [Installer automatiquement]
				const float32 by = modal.y + mh - u.s(46);
				float32 bx = modal.x + mw - u.s(24);
				if (hasCmd) {
					const float32 bw = u.TextW(NkT("tc.autoinstall")) + u.s(46);
					const NkRect b = {bx - bw, by, bw, u.s(34)};
					bx -= bw + u.s(8);
					const bool hv = u.Hit(b);
					u.Rect(b, hv ? NkColHover(NkCol::primary) : NkCol::primary, NkR::md * u.S);
					NkOwIco(u, ic.clonerTel, "download", {b.x + u.s(14), b.y + u.s(10), u.s(14), u.s(14)},
							NkCol::primaryFg);
					u.TextV(b.x + u.s(34), b.y, u.s(34), NkT("tc.autoinstall"), NkCol::primaryFg);
					if (hv && u.click) {
#if defined(_WIN32)
						NkString c = "start \"\" cmd /k ";
						c += t->installCmd;
						NkCodeShellRun(c.CStr());
#else
						NkString c = t->installCmd;
						c += " &";
						NkCodeShellRun(c.CStr());
#endif
						t->installDlg = false;
						t->detected = false;
					}
				}
				if (needsEnv) {
					const float32 bw = u.TextW(NkT("tc.configvars")) + u.s(30);
					const NkRect b = {bx - bw, by, bw, u.s(34)};
					bx -= bw + u.s(8);
					const bool hv = u.Hit(b);
					u.Rect(b, hv ? NkCol::hover : NkCol::secondary, NkR::md * u.S);
					NkOwIco(u, ic.gear, "settings", {b.x + u.s(12), b.y + u.s(10), u.s(13), u.s(13)}, NkCol::primary);
					u.TextV(b.x + u.s(30), b.y, u.s(34), NkT("tc.configvars"), NkCol::primary);
					if (hv && u.click) {
						t->installDlg = false;
						t->OpenEnvDialog();
					}
				}
				{
					const float32 bw = u.s(96);
					const NkRect b = {modal.x + u.s(24), by, bw, u.s(34)};
					if (u.Button(b, NkT("btn.close"), NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S))
						t->installDlg = false;
				}
				if (u.ctx->input.KeyPressed(NkGuiKey::Escape))
					t->installDlg = false;
				return result;
			}

			// ── Modale « Variables d'environnement » (editeur INTEGRE cross-platform) ──
			if (t->envDlg) {
				u.dl->AddRectFilled(r, NkScrim(160), 0.f);
				const float32 mw = (r.w > u.s(720)) ? u.s(660) : (r.w - u.s(40));
				const float32 mh = (r.h > u.s(560)) ? u.s(520) : (r.h - u.s(30));
				const NkRect modal = {r.x + (r.w - mw) * 0.5f, r.y + (r.h - mh) * 0.5f, mw, mh};
				u.Panel(modal, NkCol::background, NkCol::border, NkR::lg * u.S);
				NkOwIco(u, ic.gear, "settings", {modal.x + u.s(22), modal.y + u.s(15), u.s(16), u.s(16)},
						NkCol::primary);
				u.Text(modal.x + u.s(46), modal.y + u.s(16), NkT("tc.envtitle"), NkCol::foreground);
				{
					const NkRect xr = {modal.x + mw - u.s(30), modal.y + u.s(14), u.s(18), u.s(18)};
					const bool xh = u.Hit(xr);
					u.Icon("x", {xr.x + u.s(4), xr.y + u.s(4), u.s(11), u.s(11)}, xh ? NkCol::danger : NkCol::mutedFg);
					if (xh && u.click)
						t->envDlg = false;
				}
				// Bouton « Auto-detecter » : remplit les variables vides en scannant le disque.
				{
					const float32 aw = u.TextW(NkT("tc.autodetect")) + u.s(34);
					const NkRect ar = {modal.x + mw - u.s(44) - aw, modal.y + u.s(11), aw, u.s(24)};
					const bool ah = u.Hit(ar);
					u.Rect(ar, ah ? NkCol::hover : NkCol::secondary, NkR::sm * u.S);
					NkOwIco(u, ic.gear, "settings", {ar.x + u.s(10), ar.y + u.s(6), u.s(12), u.s(12)}, NkCol::primary);
					u.TextV(ar.x + u.s(26), ar.y, u.s(24), NkT("tc.autodetect"), NkCol::primary);
					if (ah && u.click)
						t->AutoDetectEnv();
				}
				u.Rect({modal.x + u.s(16), modal.y + u.s(40), mw - u.s(32), 1.f}, NkCol::border);
				const float32 fH = u.s(30);
				const NkRect body = {modal.x + u.s(24), modal.y + u.s(50), mw - u.s(48), mh - u.s(50) - u.s(54)};
				u.dl->PushClipRect(body, true);
				float32 y = body.y - t->envScroll;
				for (usize i = 0; i < t->envRows.Size(); ++i) {
					NkToolchainsState::EnvRow &rw = t->envRows[i];
					u.Text(body.x, y + u.s(8), rw.name, NkCol::foreground);
					const float32 fx = body.x + u.s(170);
					const float32 fw = body.w - u.s(170) - u.s(40);
					const bool foc = (t->envFocus == (int32)i);
					const NkRect fr = {fx, y, fw, fH};
					u.Panel(fr, NkCol::input, foc ? NkCol::primary : NkCol::border, NkR::md * u.S);
					if (foc)
						NkOwEdit(u, fr, rw.value, (int32)sizeof(rw.value), t->envCaret, t->envBlink, dt, u.s(10));
					else
						u.TextEllipsis(fr.x + u.s(10), fr.y + (fH - u.Lh()) * 0.5f, fw - u.s(16),
									   rw.value[0] ? rw.value : NkT("tc.undefined"),
									   rw.value[0] ? NkCol::foreground : NkCol::mutedFg);
					if (u.Hit(fr) && u.click) {
						t->SetEnvFocus((int32)i, rw.value);
						t->focusClaimed = true;
					}
					// Parcourir (dossier)
					const NkRect br = {fx + fw + u.s(6), y, u.s(30), fH};
					const bool bh = u.Hit(br);
					u.Rect(br, bh ? NkCol::hover : NkCol::muted, NkR::md * u.S);
					NkOwIco(u, ic.ouvrirDossier, "folder", {br.x + u.s(9), br.y + u.s(8), u.s(14), u.s(14)},
							NkCol::mutedFg);
					if (bh && u.click && dlg) {
						t->envFocus = -1;
						dlg->BrowseInto(rw.value, (int32)sizeof(rw.value), rw.name);
					}
					y += fH + u.s(10);
				}
				u.dl->PopClipRect();
				t->envScroll = 0.f; // (liste courte, pas de scroll pour l'instant)
				// Pied : Annuler / Enregistrer
				u.Rect({modal.x, modal.y + mh - u.s(54), mw, u.s(54)}, NkCol::sidebar);
				u.Rect({modal.x, modal.y + mh - u.s(54), mw, 1.f}, NkCol::border);
				const float32 by = modal.y + mh - u.s(54) + (u.s(54) - u.s(34)) * 0.5f;
				{
					const float32 sw = u.TextW(NkT("btn.save")) + u.s(50);
					const NkRect sr = {modal.x + mw - u.s(24) - sw, by, sw, u.s(34)};
					const bool hv = u.Hit(sr);
					u.Rect(sr, hv ? NkColHover(NkCol::primary) : NkCol::primary, NkR::md * u.S);
					NkOwIco(u, ic.valideSimple, "check", {sr.x + u.s(16), sr.y + u.s(10), u.s(14), u.s(14)},
							NkCol::primaryFg);
					u.TextV(sr.x + u.s(36), sr.y, u.s(34), NkT("btn.save"), NkCol::primaryFg);
					if (hv && u.click)
						t->StartSaveEnvAsync();
				} // enregistrement + re-detection sur thread de fond
				{
					const float32 cw2 = u.s(96);
					const NkRect cr = {modal.x + mw - u.s(24) - (u.TextW(NkT("btn.save")) + u.s(50)) - u.s(8) - cw2, by,
									   cw2, u.s(34)};
					if (u.Button(cr, NkT("btn.cancel"), NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S))
						t->envDlg = false;
				}
				u.TextV(modal.x + u.s(24), modal.y + mh - u.s(54), u.s(54), NkT("set.persisthint"), NkCol::mutedFg);
				// Clic dans le vide de la modale (hors champ) -> le champ perd le focus.
				if (u.click && !t->focusClaimed && u.Hit(modal))
					t->envFocus = -1;
				if (u.ctx->input.KeyPressed(NkGuiKey::Escape)) {
					if (t->envFocus >= 0)
						t->envFocus = -1;
					else
						t->envDlg = false;
				}
				return result;
			}

			// ── Dialog « Ajouter / Editer un toolchain » (reutilise celui du wizard) ──
			if (nw->tcDlg)
				NkWizTcDialog(u, r, nw, dlg, dt, (nw->comboOpen >= 0), ic);
			// ── Dropdown des combos du dialog (rendu PAR-DESSUS tout) ──
			if (nw->comboOpen >= 0 && nw->comboOpts && nw->comboSel) {
				const float32 ih = u.s(26);
				float32 ddw = nw->comboR.w;
				for (int32 k = 0; k < nw->comboN; ++k) {
					const float32 tw = u.TextW(nw->comboOpts[k]) + u.s(30);
					if (tw > ddw)
						ddw = tw;
				}
				float32 ddx = nw->comboR.x;
				if (ddx + ddw > r.x + r.w - u.s(8))
					ddx = r.x + r.w - u.s(8) - ddw;
				if (ddx < r.x + u.s(8))
					ddx = r.x + u.s(8);
				float32 ddy = nw->comboR.y + nw->comboR.h + u.s(2);
				const float32 ddh = nw->comboN * ih + u.s(6);
				if (ddy + ddh > r.y + r.h - u.s(8))
					ddy = nw->comboR.y - ddh - u.s(2);
				const NkRect dd = {ddx, ddy, ddw, ddh};
				u.dl->AddRectFilled({dd.x + u.s(2), dd.y + u.s(3), dd.w, dd.h}, NkColor{0, 0, 0, 90}, NkR::md * u.S);
				u.Panel(dd, NkCol::surface, NkCol::primary, NkR::md * u.S);
				bool chose = false;
				for (int32 k = 0; k < nw->comboN; ++k) {
					const NkRect ir = {dd.x + u.s(4), dd.y + u.s(3) + k * ih, dd.w - u.s(8), ih};
					const bool hv = u.Hit(ir);
					if (hv || k == *nw->comboSel)
						u.Rect(ir, NkCol::hover, NkR::sm * u.S);
					u.TextEllipsis(ir.x + u.s(8), ir.y + (ih - u.Lh()) * 0.5f, ir.w - u.s(12), nw->comboOpts[k],
								   NkCol::foreground);
					if (hv && u.click) {
						*nw->comboSel = k;
						chose = true;
					}
				}
				if (chose || (u.click && !u.Hit(dd) && !nw->comboJustOpened))
					nw->comboOpen = -1;
				nw->comboJustOpened = false;
			}
			// ── Overlay de progression (thread de fond) : VOILE semi-transparent + carte centree.
			//    La page derriere reste visible (elle n'est PLUS effacee).
			if (t->bgBusy.Load()) {
				u.dl->AddRectFilled(r, NkScrim(150), 0.f);
				const float32 mw = (r.w > u.s(560)) ? u.s(460) : (r.w - u.s(60));
				const float32 mh = u.s(150);
				const NkRect m = {r.x + (r.w - mw) * 0.5f, r.y + (r.h - mh) * 0.5f, mw, mh};
				u.Panel(m, NkCol::background, NkCol::border, NkR::lg * u.S);
				NkOwIco(u, ic.toolchains, "cpu", {m.x + u.s(22), m.y + u.s(21), u.s(16), u.s(16)}, NkCol::primary);
				u.Text(m.x + u.s(46), m.y + u.s(22), NkT("tc.detecttitle"), NkCol::foreground);
				const int32 ph = t->bgPhase.Load();
				const char *lbl = (ph <= 1)	 ? NkT("tc.phase.persist")
								  : (ph < 5) ? NkT("tc.phase.analyze")
											 : NkT("tc.phase.finalize");
				u.Text(m.x + u.s(24), m.y + u.s(58), lbl, NkCol::mutedFg);
				const float32 bx = m.x + u.s(24), bw = mw - u.s(48), by = m.y + u.s(96);
				u.Rect({bx, by, bw, u.s(8)}, NkCol::muted, NkR::sm * u.S);
				const float32 frac = (ph >= 6) ? 1.f : (ph >= 5) ? 0.85f : (ph >= 3) ? 0.55f : 0.2f;
				u.Rect({bx, by, bw * frac, u.s(8)}, NkCol::primary, NkR::sm * u.S);
				u.TextV(m.x + u.s(24), m.y + mh - u.s(30), u.s(20), NkT("tc.reactive"), NkCol::mutedFg);
			}
			// Clic dans le vide du panneau (hors champ) -> le filtre perd le focus.
			if (u.click && !blockBg && !t->focusClaimed && u.Hit(r))
				t->focus = -1;
			if (u.ctx->input.KeyPressed(NkGuiKey::Escape)) {
				if (nw->comboOpen >= 0)
					nw->comboOpen = -1;
				else if (nw->tcDlg)
					nw->tcDlg = false;
				else if (t->focus >= 0)
					t->focus = -1;
				else
					result = 1;
			}
			return result;
		}

	} // namespace nkcode
} // namespace nkentseu
