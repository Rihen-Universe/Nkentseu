#pragma once
// =============================================================================
// NkNewWorkspace.h — Assistant « Nouveau Workspace » (wizard 5 etapes) du
// launcher NKCode. D'apres le design Banani « NKCode IDE » (composants Wizard*)
// + la spec textuelle (sections 4 & 5).
//
// Mode PLEIN CADRE du launcher (meme barre de titre + sidebar nav, item
// « Nouveau Workspace » actif) ; le panneau central est remplace par ce wizard.
//
// PHASE 1 = MAQUETTE VISUELLE : barre d'etapes + navigation + Etape 1 complete ;
// Etapes 2..5 = pages titrees (a etoffer). Le fonctionnel (generation .jenga +
// creation des fichiers) viendra APRES la validation visuelle.
//
// L'etat (NkNewWsState) vit dans NkHomeState ; le panneau renvoie une action
// (0 = rien, 1 = annuler -> retour Accueil).
// =============================================================================
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Shell/NkOpenWs.h"        // reutilise NkOwEdit (editeur caret) + NkOwIco
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/Dialogs.h"
#include <cstdio>
#include <cstdlib>   // std::getenv (detection reelle des toolchains)

namespace nkentseu {
namespace nkcode {

    // Table des systemes/plateformes supportes. special : 0 aucun, 1 android, 2 web, 3 apple.
    struct NkOsDef { const char* name; const char* filt; int32 special; };
    inline const NkOsDef* NkWizOsTable(int32* n) {
        static const NkOsDef T[] = {
            { "WINDOWS",   "system:Windows",   0 },
            { "LINUX",     "system:Linux",     0 },
            { "MACOS",     "system:macOS",     3 },
            { "ANDROID",   "system:Android",   1 },
            { "IOS",       "system:iOS",       3 },
            { "WEB",       "system:Web",       2 },
            { "HARMONYOS", "system:HarmonyOS", 0 },
            { "XBOX",      "system:Xbox",      0 },
            { "SWITCH",    "system:Switch",    0 },
            { "PS5",       "system:PS5",       0 },
        };
        *n = 10; return T;
    }
    inline int32 NkWizOsCount() { int32 n = 0; (void)NkWizOsTable(&n); return n; }
    inline const char* NkWizOsName(int32 i) { int32 n = 0; const NkOsDef* T = NkWizOsTable(&n); return (i >= 0 && i < n) ? T[i].name : ""; }
    inline const char* NkWizOsFilt(int32 i) { int32 n = 0; const NkOsDef* T = NkWizOsTable(&n); return (i >= 0 && i < n) ? T[i].filt : ""; }
    inline int32       NkWizOsSpecial(int32 i) { int32 n = 0; const NkOsDef* T = NkWizOsTable(&n); return (i >= 0 && i < n) ? T[i].special : 0; }

    // ── Un projet defini dans le wizard (etape 2 / dialog) ──
    struct NkWizProject {
        char     name[80] = "MonApp";
        int32    kind = 0;            // 0 consoleapp, 1 windowedapp, 2 staticlib, 3 sharedlib, 4 test
        int32    lang = 1;            // 0 C, 1 C++, 2 ObjC, 3 ObjC++, 4 Zig, 5 Rust
        int32    dialect = 3;         // index dans la table des dialectes
        char     location[200] = "app/";
        NkVector<NkString> sources;
        NkVector<NkString> includes;
        NkVector<NkString> dependsOn;
        bool     genMain = true, genReadme = true, genTest = false;
        // Proprietes des tests (bloc « with test(): testfiles([...]) » a la maniere Jenga)
        char     testFiles[160] = "tests/**.cpp";
        bool     testDesktopOnly = true;
        bool     isStart = false;
        bool     dynVars = true;            // repertoires de build via variables dynamiques Jenga
        bool     separateFile = false;      // .jenga separe (avec imports) vs integre au workspace
        char     objDir[220] = "%{wks.location}/Build/Obj/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}";
        char     tgtDir[220] = "%{wks.location}/Build/Bin/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}";
        char     binDir[220] = "%{wks.location}/Build/Bin/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}";
        // — Filtres par plateforme : 1 entree par systeme supporte (voir NkWizOsTable). `on` = filtre present. —
        struct OsFlt { bool on = false; int32 toolchain = -1; NkVector<NkString> links, defines; int32 androidKind = 1; int32 webMem = 1; };
        OsFlt    os[14];
        // — Filtres de configuration : un par config du workspace (Debug/Release/custom coches) —
        struct CfgFlt { NkString name; NkVector<NkString> defines; int32 opt = 0; bool sym = true; };
        NkVector<CfgFlt> cfgFlt;
        // — Filtres personnalises (expression libre) —
        struct CustFlt { char expr[120] = {}; int32 toolchain = -1; NkVector<NkString> defines, links; bool ok = false; };
        NkVector<CustFlt> custFlt;
        // — Code .jenga edite manuellement (conserve a la creation, reste editable) —
        NkString customCode; bool hasCustomCode = false;
    };

    // ── Etat complet du wizard ──
    struct NkNewWsState {
        int32    step = 0;            // 0..4 (5 etapes)
        bool     inited = false;
        // Edition de texte partagee (un seul champ focus a la fois).
        int32    focus = -1, caret = 0; float32 blink = 0.f;
        // Combo deroulant ouvert (rendu differe en fin de panneau).
        int32    comboOpen = -1; NkRect comboR{}; const char* const* comboOpts = nullptr; int32 comboN = 0; int32* comboSel = nullptr;
        bool     comboJustOpened = false;   // ignore la frame d'ouverture pour la fermeture au clic exterieur
        // Defilement du corps du wizard (vertical + horizontal).
        float32  scroll = 0.f, scrollMax = 0.f, barOff = 0.f; bool barDrag = false;
        float32  hscroll = 0.f, hscrollMax = 0.f, hbarOff = 0.f; bool hbarDrag = false;
        // — Etape 1 : Workspace —
        char     wsName[120]   = "MonWorkspace";
        char     location[400] = {};       // dossier PARENT ou creer le workspace
        char     jengaFile[160] = {};
        bool     jengaManual = false;
        char     locBase[400] = {};
        bool     cfgDebug = true, cfgRelease = true;
        struct CustomCfg { NkString name; bool on = true; };
        NkVector<CustomCfg> customCfgs;        // configs ajoutees : cochables (on/off), pas retirees au clic
        char     newCfgName[60] = "Profiling";
        int32    newCfgBase = 1;            // 0 Debug, 1 Release (combo « Basee sur »)
        bool     gitInit = true, gitIgnore = true, gitCommit = false;
        // — Navigateur de dossiers (bouton Parcourir) —
        NkOpenWsState picker; bool picking = false;
        // — Etape 2 : Projets —
        NkVector<NkWizProject> projects;
        int32    startProj = -1;
        int32    dragProj = -1;                // reordonnancement (ordre de build) : index glisse
        // dialog « Ajouter / Editer un projet » (modale large, 3 etapes internes)
        bool     projDlg = false; int32 projEditIdx = -1; int32 projStep = 0; NkWizProject projDraft;
        char     projSrcAdd[160] = {}, projIncAdd[160] = {};
        char     projFltAdd[160] = {}; int32 projFltTarget = -1;   // ajout dans une liste de filtre (links/defines)
        float32  projScroll = 0.f, projScrollMax = 0.f;
        bool     projAdvanced = false;
        // Visualiseur de code (apercu filtres + extrait .jenga) : scroll V/H propre.
        float32  codeSY = 0.f, codeSX = 0.f, codeBarOff = 0.f; int32 codeBar = 0;
        // Etape Filtres : scroll V/H independant pour la colonne gauche et la colonne droite.
        float32  fltLSY = 0.f, fltLSX = 0.f, fltRSY = 0.f, fltRSX = 0.f;
        // Etape Filtres : zone dediee « filtres de configuration » avec son propre scroll V/H.
        float32  cfgSY = 0.f, cfgSX = 0.f;
        // Transitoire : un champ a-t-il capture le clic cette frame ? (sinon clic dans le vide = defocus)
        bool     focusClaimed = false;
        // Etape Resume : edition manuelle de l'extrait .jenga.
        NkString extractBuf; bool extractEditing = false, extractEdited = false; int32 extractCaret = 0;
        void OpenProjDlg(int32 editIdx) {
            projDlg = true; projEditIdx = editIdx; projStep = 0; focus = -1; comboOpen = -1; projScroll = 0.f;
            projSrcAdd[0] = projIncAdd[0] = projFltAdd[0] = '\0'; projFltTarget = -1;
            fltLSY = fltLSX = fltRSY = fltRSX = 0.f; codeSX = codeSY = 0.f; cfgSY = cfgSX = 0.f;
            extractEditing = false; extractEdited = false; extractCaret = 0; extractBuf.Clear();
            if (editIdx >= 0 && editIdx < (int32)projects.Size()) projDraft = projects[editIdx];
            else { NkWizProject p; p.sources.PushBack(NkString("src/**.cpp")); p.sources.PushBack(NkString("src/**.c"));
                   p.includes.PushBack(NkString("include")); projDraft = p; }
            // Filtres systeme presents par defaut = plateformes cochees au workspace (modifiable ensuite).
            { int32 nOs = NkWizOsCount(); bool anyOn = false; for (int32 i = 0; i < nOs; ++i) if (projDraft.os[i].on) { anyOn = true; break; }
              if (!anyOn) { const bool en[7] = { osWin, osLinux, osMac, osAndroid, osIos, osWeb, osHarmony };
                            for (int32 i = 0; i < nOs; ++i) projDraft.os[i].on = (i < 7) ? en[i] : false; } }
            // Reprendre l'edition manuelle existante.
            if (projDraft.hasCustomCode) { extractBuf = projDraft.customCode; extractEdited = true; }
            // Filtres de config : un par config du workspace (cochee). Seeds par defaut Debug/Release.
            if (projDraft.cfgFlt.Empty()) {
                auto addCfg = [&](const char* name, bool isDbg) {
                    NkWizProject::CfgFlt c; c.name = name;
                    if (isDbg)      { c.opt = 0; c.sym = true;  c.defines.PushBack(NkString("_DEBUG")); c.defines.PushBack(NkString("DEBUG")); }
                    else            { c.opt = 2; c.sym = false; c.defines.PushBack(NkString("NDEBUG")); }
                    projDraft.cfgFlt.PushBack(c);
                };
                if (cfgDebug)   addCfg("Debug", true);
                if (cfgRelease) addCfg("Release", false);
                for (usize i = 0; i < customCfgs.Size(); ++i) if (customCfgs[i].on) {
                    NkWizProject::CfgFlt c; c.name = customCfgs[i].name; projDraft.cfgFlt.PushBack(c);
                }
            }
        }
        void CommitProjDlg() {
            if (!projDraft.name[0] || !ValidName(projDraft.name)) return;
            // On ne depend QUE d'une bibliotheque existante : on purge tout residu (app cochee, projet supprime...).
            for (int32 k = (int32)projDraft.dependsOn.Size() - 1; k >= 0; --k) {
                bool isLib = false;
                for (usize i = 0; i < projects.Size(); ++i) if ((int32)i != projEditIdx && StrEq(projects[i].name, projDraft.dependsOn[k].CStr()) && (projects[i].kind == 2 || projects[i].kind == 3)) { isLib = true; break; }
                if (!isLib) projDraft.dependsOn.Erase(projDraft.dependsOn.Begin() + k);
            }
            // L'extrait edite manuellement est CONSERVE a la creation (et reste re-editable).
            if (extractEdited) { projDraft.customCode = extractBuf; projDraft.hasCustomCode = true; }
            if (projEditIdx >= 0 && projEditIdx < (int32)projects.Size()) projects[projEditIdx] = projDraft;
            else projects.PushBack(projDraft);
            projDlg = false;
            extractEditing = false; extractEdited = false; extractBuf.Clear();
        }
        // — Etape 3 : Toolchains (DETECTION REELLE sur ce systeme) —
        bool     tcAuto = true;                                   // RegisterJengaGlobalToolchains()
        struct TcDet { NkString name, version, target, path; bool found = false; };
        NkVector<TcDet> tcDet;                                    // rempli par DetectToolchains()
        bool     tcDetDone = false;
        bool     tcSel[16] = {};                                  // toolchains coches (init = detecte)
        const char* tcNamePtrs[16] = {}; int32 tcNameN = 0;       // noms stables pour les combos
        // Assignation par plateforme : chaque plateforme ne liste QUE les toolchains qui la ciblent.
        const char* tcPlatPtrs[7][16] = {}; int32 tcPlatN[7] = {}; int32 tcPlatSel[7] = {};
        // Toolchain personnalise (dialog « Ajouter manuel »)
        struct CustomTc {
            char name[64] = "mon-gcc-12"; int32 family = 1; int32 os = 1; int32 arch = 0; int32 env = 0;
            char triple[96] = "x86_64-unknown-linux-gnu";
            char cc[220] = "", cxx[220] = "", ar[220] = "", ld[220] = "", sysroot[220] = "";
            NkVector<NkString> cflags, cxxflags, ldflags; bool regGlobal = true;
        };
        NkVector<CustomTc> customTc;
        bool     tcDlg = false; CustomTc tcDraft; char tcFltAdd[120] = {}; int32 tcFltTarget = -1;
        int32    tcEditIdx = -1;   // -1 = nouveau toolchain ; >=0 = edition de customTc[idx]
        float32  tcDlgScroll = 0.f, tcDlgScrollMax = 0.f, tcDlgScrollX = 0.f;
        bool     tcTested = false;   // « Lancer la verification » a-t-il ete clique ? (sinon pas de resultat)
        float32  tcTestScroll = 0.f; // defilement de la zone de resultats de test
        // — Etape 4 : Plateformes & Architectures —
        bool     osWin = true, osLinux = true, osMac = false, osAndroid = true, osWeb = true,
                 osIos = false, osHarmony = false;
        bool     osFreeBSD = false, osTvos = false, osWatchos = false,
                 osXboxOne = false, osXboxSeries = false, osPs4 = false, osPs5 = false, osSwitch = false;
        bool     archX64 = true, archArm64 = true, archWasm32 = true, archX86 = false, archArm = false,
                 archWasm64 = false, archRiscv64 = false, archMips = false;
        // ABIs Android + niveaux d'API + identite
        bool     abiArm64v8a = true, abiX86_64 = true, abiArmeabi = false, abiX86 = false;
        int32    androidApiMin = 1, androidApiTarget = 6, androidCompileSdk = 6;   // index dans NkWizAndroidApi
        char     androidAppId[120] = "com.exemple.monapp";
        // Options Emscripten
        int32    emInitMem = 2, emStackSize = 3;   // index
        char     emExportName[80] = "Module";
        float32  step4Scroll = 0.f;
        // — Etape 5 : edition manuelle de l'apercu workspace (variables DEDIEES, distinctes de la modale projet) —
        NkString wsExtractBuf; bool wsExtractEditing = false, wsExtractEdited = false; int32 wsExtractCaret = 0;
        int32    wsTab = 0;   // onglet actif de l'apercu : 0 = workspace, 1+ = projets en fichier separe

        void EnsureInit(NkCodeState* st) {
            if (inited) return; inited = true; (void)st;
            const NkString home = NkOpenWsState::Home();
            NkString base = NkOpenWsState::DefaultProjectDir();   // reglage Parametres > Chemins (projDir), sinon ~/Projects
            if (!NkDirectory::Exists(base.CStr())) base = home;
            int32 n = 0; for (; base.CStr()[n] && n + 1 < (int32)sizeof(locBase); ++n) locBase[n] = base.CStr()[n]; locBase[n] = '\0';
            std::snprintf(location, sizeof(location), "%s", locBase);   // emplacement = PARENT par defaut
            SyncDerived();
        }
        // jengaFile derive du nom (sauf override manuel). L'emplacement reste le PARENT saisi.
        void SyncDerived() { if (!jengaManual) std::snprintf(jengaFile, sizeof(jengaFile), "%s.jenga", wsName); }
        // Dossier final = <emplacement>/<NomWorkspace> (toujours append le nom).
        NkString FinalFolder() const { return (NkPath(location) / wsName).ToString(); }

        // ── Detection REELLE des toolchains (PATH + variables d'environnement + WSL2) ──
        static NkString TcEnv(const char* n) { const char* v = std::getenv(n); return v ? NkString(v) : NkString(); }
        static NkString TcWhich(const char* exe) {
            const char* path = std::getenv("PATH"); if (!path) return NkString();
#if defined(_WIN32)
            const char sep = ';';
#else
            const char sep = ':';
#endif
            NkString dir;
            for (const char* p = path; ; ++p) {
                if (*p == sep || *p == '\0') {
                    if (!dir.Empty()) {
#if defined(_WIN32)
                        static const char* exts[] = { ".exe", ".cmd", ".bat" };
                        for (const char* e : exts) { NkString full = dir; full += "\\"; full += exe; full += e; if (NkFile::Exists(full.CStr())) return full; }
#else
                        NkString full = dir; full += "/"; full += exe; if (NkFile::Exists(full.CStr())) return full;
#endif
                    }
                    dir.Clear(); if (*p == '\0') break;
                } else dir += *p;
            }
            return NkString();
        }
        // Execute une commande et capture sa sortie standard (detection unique ; bref flash console sous Windows).
        static NkString TcRun(const char* cmd) {
            NkString out;
#if defined(_WIN32)
            FILE* p = _popen(cmd, "r");
#else
            FILE* p = popen(cmd, "r");
#endif
            if (!p) return out;
            // Lecture OCTET PAR OCTET en ignorant les octets nuls : robuste a l'UTF-16LE
            // (sortie par defaut de wsl.exe) qui casserait un fgets/strlen au 1er '\0'.
            int ch; while ((ch = std::fgetc(p)) != EOF) { if (ch != 0) out += (char)ch; }
#if defined(_WIN32)
            _pclose(p);
#else
            pclose(p);
#endif
            return out;
        }
        // Le nom correspond-il a un toolchain enregistre par RegisterJengaGlobalToolchains ?
        static bool IsJengaTc(const char* n) {
            static const char* T[] = { "android-ndk", "clang-cross-linux", "clang-mingw", "emscripten", "gcc-cross-linux", "host-apple-clang", "host-clang", "host-gcc", "mingw", "msvc", "ohos-ndk" };
            for (const char* t : T) if (StrEq(n, t)) return true;
            return false;
        }
        void DetectToolchains() {
            tcDet = NkVector<TcDet>();
            auto add = [&](const char* nm, const char* ver, const char* tgt, const NkString& pth, bool found) {
                TcDet d; d.name = nm; d.version = ver; d.target = tgt; d.path = found ? pth : NkString("introuvable"); d.found = found; tcDet.PushBack(d);
            };
#if defined(_WIN32)
            const char* hostOs = "Windows x64";
#elif defined(__APPLE__)
            const char* hostOs = "macOS host";
#else
            const char* hostOs = "Linux x86_64";
#endif
            // Noms = toolchains REELS de RegisterJengaGlobalToolchains (sinon usetoolchain echoue).
            { NkString p = TcWhich("clang"); if (p.Empty()) p = TcWhich("clang++");
#if defined(_WIN32)
              add("clang-mingw", "Clang (MinGW/PATH)", hostOs, p, !p.Empty());
#elif defined(__APPLE__)
              add("host-apple-clang", "Apple Clang", hostOs, p, !p.Empty());
#else
              add("host-clang", "Clang (PATH)", hostOs, p, !p.Empty());
#endif
            }
#if defined(_WIN32)
            { NkString p = TcWhich("gcc"); if (p.Empty() && NkFile::Exists("C:\\msys64\\ucrt64\\bin\\gcc.exe")) p = NkString("C:\\msys64\\ucrt64\\bin\\gcc.exe");
              add("mingw", "GCC (MinGW/UCRT64)", "Windows x64", p, !p.Empty()); }
#else
            { NkString p = TcWhich("gcc"); add("host-gcc", "GCC (PATH)", hostOs, p, !p.Empty()); }
#endif
            { NkString p = TcEnv("ANDROID_NDK_ROOT"); if (p.Empty()) p = TcEnv("ANDROID_NDK_HOME");
              if (p.Empty()) { NkString ah = TcEnv("ANDROID_HOME"); if (!ah.Empty()) { NkString c = ah; c += "/ndk"; if (NkDirectory::Exists(c.CStr())) p = c; } }
              add("android-ndk", "Android NDK", "Android ARM64", p, !p.Empty()); }
            { NkString p = TcEnv("EMSDK"); if (p.Empty()) p = TcWhich("emcc"); add("emscripten", "Emscripten", "Web WASM32", p, !p.Empty()); }
            { NkString p = TcWhich("zig"); add("zig", "Zig (cross-compile)", "Linux / Win / macOS", p, !p.Empty()); }
            { NkString p = TcWhich("clang-cl"); add("msvc", "MSVC clang-cl", "Windows MSVC", p, !p.Empty()); }
            { NkString p = TcEnv("OHOS_SDK"); if (p.Empty()) p = TcEnv("OHOS_NDK_HOME"); if (p.Empty()) p = TcEnv("DEVECO_SDK_HOME"); if (p.Empty()) p = TcEnv("HARMONY_HOME");
              add("ohos-ndk", "OHOS / HarmonyOS NDK", "HarmonyOS ARM64", p, !p.Empty()); }
#if defined(_WIN32)
            // WSL est 100% OPTIONNEL : si wsl.exe est absent, on n'ajoute rien et on n'execute rien (aucun risque).
            if (NkFile::Exists("C:\\Windows\\System32\\wsl.exe")) {
              {
                  // Interroge WSL : distro + architecture + compilateurs reellement installes dedans.
                  // NB1: `command -v a b c` ne gere qu'UN argument sous dash -> on boucle sur chaque outil.
                  // NB2: wsl.exe sort en UTF-16LE par defaut (-> caracteres « chinois ») : WSL_UTF8=1 force l'UTF-8.
                  _putenv("WSL_UTF8=1");
                  NkString oRaw = TcRun("wsl.exe -e sh -c \"grep -h '^PRETTY_NAME=' /etc/os-release 2>/dev/null; uname -m; echo @@; for c in gcc clang g++ clang++ cc; do command -v $c 2>/dev/null; done\"");
                  // Securite : on retire tout octet non-ASCII imprimable (residu d'encodage) sauf les sauts de ligne.
                  NkString o; for (const char* c = oRaw.CStr(); *c; ++c) { const unsigned char ch = (unsigned char)*c; if (ch == '\n' || (ch >= 32 && ch < 127)) o += (char)ch; }
                  NkString distro, arch, comps; bool afterMark = false, gotArch = false;
                  NkString line;
                  for (const char* s = o.CStr(); ; ++s) {
                      if (*s == '\n' || *s == '\0') {
                          NkString ln; for (const char* c = line.CStr(); *c && *c != '\r'; ++c) ln += *c;
                          if (ln == "@@") afterMark = true;
                          else if (!afterMark) {
                              if (ln.Contains("PRETTY_NAME=")) { const char* a = nullptr; const char* b = nullptr;
                                  for (const char* c = ln.CStr(); *c; ++c) if (*c == '"') { if (!a) a = c + 1; else { b = c; break; } }
                                  if (a && b) for (const char* c = a; c < b; ++c) distro += *c; }
                              else if (!ln.Empty() && !gotArch) { arch = ln; gotArch = true; }
                          } else if (!ln.Empty()) {
                              const char* base = ln.CStr(); for (const char* c = ln.CStr(); *c; ++c) if (*c == '/') base = c + 1;
                              if (!comps.Empty()) comps += ", "; comps += base;
                          }
                          line.Clear(); if (*s == '\0') break;
                      } else line += *s;
                  }
                  if (distro.Empty()) distro = "Linux";
                  // Cible CONTENANT « Linux » -> apparait dans le filtre plateforme Linux.
                  NkString tgt = "Linux "; if (!arch.Empty()) { tgt += arch; tgt += " "; } tgt += "(WSL2 "; tgt += distro; tgt += ")";
                  const bool hasGcc = comps.Contains("gcc") || comps.Contains("g++");
                  const bool hasClang = comps.Contains("clang");
                  // Une toolchain WSL par famille de compilateur -> compile du Linux NATIF depuis Windows.
                  if (hasGcc)   add("wsl2-gcc",   "GCC via WSL2 (gcc/g++)",     tgt.CStr(), NkString("wsl.exe -e g++"),     true);
                  if (hasClang) add("wsl2-clang", "Clang via WSL2 (clang/clang++)", tgt.CStr(), NkString("wsl.exe -e clang++"), true);
                  if (!hasGcc && !hasClang) add("wsl2-linux", "WSL2 - aucun compilateur installe", tgt.CStr(), NkString("C:\\Windows\\System32\\wsl.exe"), false);
              } }
#endif
            for (int32 i = 0; i < (int32)tcDet.Size() && i < 16; ++i) tcSel[i] = tcDet[i].found;
            RefreshTcPtrs();
            tcDetDone = true;
        }
        // Recalcule les pointeurs de noms (combos/aperçu) depuis tcDet : a appeler a chaque frame
        // qui les utilise -> evite tout pointeur CStr() devenu invalide (anti-crash).
        void RefreshTcPtrs() {
            tcNameN = (int32)tcDet.Size(); if (tcNameN > 16) tcNameN = 16;
            for (int32 i = 0; i < tcNameN; ++i) tcNamePtrs[i] = tcDet[i].name.CStr();
            const char* pkeys[7] = { "Windows", "Linux", "macOS", "Android", "iOS", "Web", "HarmonyOS" };
            for (int32 pp = 0; pp < 7; ++pp) {
                int32 n = 0;
                for (int32 i = 0; i < (int32)tcDet.Size() && n < 16; ++i)
                    if (tcDet[i].found && tcDet[i].target.Contains(pkeys[pp])) tcPlatPtrs[pp][n++] = tcDet[i].name.CStr();
                tcPlatN[pp] = n; if (tcPlatSel[pp] >= n) tcPlatSel[pp] = 0;
            }
        }

        static bool ValidName(const char* s) {
            if (!s || !*s) return false;
            for (const char* p = s; *p; ++p) { const char c = *p;
                if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return false; }
            return true;
        }
        bool Step1Valid() const {
            if (!ValidName(wsName)) return false;
            if (!jengaFile[0] || !ValidName(jengaFile) || !NkCodeState::EndsWithI(jengaFile, ".jenga")) return false;
            if (!location[0]) return false;
            return true;
        }
        void AddCustomCfg() {
            if (!newCfgName[0] || !ValidName(newCfgName)) return;
            if (StrEq(newCfgName, "Debug") || StrEq(newCfgName, "Release")) return;   // doublon
            for (usize i = 0; i < customCfgs.Size(); ++i) if (StrEq(customCfgs[i].name.CStr(), newCfgName)) { customCfgs[i].on = true; return; }
            CustomCfg c; c.name = newCfgName; c.on = true; customCfgs.PushBack(c);
        }
        void SetFocus(int32 id, const char* buf) {
            focus = id; blink = 0.f; int32 n = 0; while (buf[n]) ++n; caret = n;
        }

        // ── Generation du contenu .jenga depuis l'etat du wizard ──
        NkString BuildJenga() const {
            NkString s;
            // En-tete (« guards ») Python : shebang + encodage + docstring.
            s += "#!/usr/bin/env python3\n";
            s += "# -*- coding: utf-8 -*-\n";
            s += "\"\"\""; s += wsName; s += " - workspace Jenga (genere par NKCode).\"\"\"\n\n";
            if (osAndroid) s += "import os\n";   // pour os.getenv(...) des chemins SDK/NDK
            s += "from Jenga import *\n";
            s += "from Jenga.GlobalToolchains import RegisterJengaGlobalToolchains\n\n";
            s += "with workspace(\""; s += wsName; s += "\"):\n";
            if (tcAuto) s += "    RegisterJengaGlobalToolchains()\n";
            // configurations
            { bool first = true; s += "    configurations([";
              auto addc = [&](const char* n) { if (!first) s += ", "; s += "\""; s += n; s += "\""; first = false; };
              if (cfgDebug) addc("Debug"); if (cfgRelease) addc("Release");
              for (usize i = 0; i < customCfgs.Size(); ++i) if (customCfgs[i].on) addc(customCfgs[i].name.CStr());
              s += "])\n"; }
            // targetoses : UNIQUEMENT les systemes coches a l'etape 4.
            s += "    targetoses([\n";
            { auto addos = [&](bool on, const char* e) { if (on) { s += "        TargetOS."; s += e; s += ",\n"; } };
              addos(osWin, "WINDOWS"); addos(osLinux, "LINUX"); addos(osMac, "MACOS"); addos(osFreeBSD, "FREEBSD");
              addos(osAndroid, "ANDROID"); addos(osIos, "IOS"); addos(osTvos, "TVOS"); addos(osWatchos, "WATCHOS"); addos(osHarmony, "HARMONYOS");
              addos(osWeb, "WEB");
              addos(osXboxOne, "XBOX_ONE"); addos(osXboxSeries, "XBOX_SERIES"); addos(osPs4, "PS4"); addos(osPs5, "PS5"); addos(osSwitch, "SWITCH"); }
            s += "    ])\n";
            // targetarchs : UNIQUEMENT les architectures cochees.
            s += "    targetarchs([\n";
            { auto adda = [&](bool on, const char* e) { if (on) { s += "        TargetArch."; s += e; s += ",\n"; } };
              adda(archX64, "X86_64"); adda(archArm64, "ARM64"); adda(archWasm32, "WASM32"); adda(archX86, "X86");
              adda(archArm, "ARM"); adda(archRiscv64, "RISCV64"); adda(archWasm64, "WASM64"); adda(archMips, "MIPS"); }
            s += "    ])\n";
            if (startProj >= 0 && startProj < (int32)projects.Size()) { s += "    startproject(\""; s += projects[startProj].name; s += "\")\n"; }
            // NB: l'assignation toolchain par plateforme se fait au niveau PROJET (cf. BuildProjectJenga),
            //     car usetoolchain() + with filter() au niveau workspace ne respecte pas le filtre.
            // Android : chemins SDK/NDK au niveau WORKSPACE (les ABIs/API/appId sont au niveau PROJET, cf. BuildProjectJenga).
            if (osAndroid) {
                s += "    androidsdkpath(os.getenv(\"ANDROID_SDK_ROOT\", \"\"))\n";
                s += "    androidndkpath(os.getenv(\"ANDROID_NDK_ROOT\", \"\"))\n";
            }
            // Web/Emscripten : memoire initiale.
            if (osWeb) {
                static const int32 MEMV[] = { 16, 32, 64, 128, 256 };
                char mb[12]; std::snprintf(mb, sizeof(mb), "%d", (emInitMem >= 0 && emInitMem < 5) ? MEMV[emInitMem] : 32);
                s += "    with filter(\"system:Web\"):\n        emscripteninitialmemory("; s += mb; s += ")\n";
            }
            // Framework de tests : un bloc unitest() est requis des qu'un projet genere des tests.
            { bool anyTest = false; for (usize i = 0; i < projects.Size(); ++i) if (projects[i].genTest && projects[i].kind != 4) { anyTest = true; break; }
              if (anyTest) s += "\n    with unitest() as u:\n        u.Compile()\n"; }
            // Projets en fichier .jenga SEPARE : inclus via with include("...").
            for (usize i = 0; i < projects.Size(); ++i) {
                const NkWizProject& p = projects[i];
                if (!p.separateFile) continue;
                s += "\n    with include(\""; s += p.location; s += p.name; s += ".jenga\"):\n        pass\n";
            }
            // Projets INTEGRES : bloc complet (filtres compris), edition manuelle conservee, reindente sous le workspace.
            for (usize i = 0; i < projects.Size(); ++i) {
                const NkWizProject& p = projects[i];
                if (p.separateFile) continue;
                const NkString block = p.hasCustomCode ? p.customCode : BuildProjectJenga(p);
                s += "\n";
                NkString line;
                for (const char* c = block.CStr(); ; ++c) {
                    if (*c == '\n' || *c == '\0') { if (!line.Empty()) { s += "    "; s += line; } s += "\n"; line.Clear(); if (*c == '\0') break; }
                    else if (*c != '\r') line += *c;
                }
            }
            return s;
        }
        // Genere le bloc « with project(...) » d'un projet (avec filtres). Indentation de base = 0.
        NkString BuildProjectJenga(const NkWizProject& p) const {
            static const char* KINDS[] = { "consoleapp", "windowedapp", "staticlib", "sharedlib", "test" };
            static const char* LANGS[] = { "C", "C++", "ObjC", "ObjC++", "Zig", "Rust" };
            static const char* DIALS[] = { "C++11", "C++14", "C++17", "C++20", "C++23" };
            static const char* TCS[]   = { "(auto)", "clang-mingw", "zig-linux-x64", "android-ndk", "emscripten", "clang-native", "clang-cl" };
            static const char* OPT[]   = { "Off", "Size", "Speed", "Full" };
            static const int32 MEM[]   = { 16, 32, 64, 128, 256 };
            auto lst = [](NkString& s, const NkVector<NkString>& v, const char* prefix) {
                s += "["; for (usize i = 0; i < v.Size(); ++i) { if (i) s += ", "; s += "\""; s += prefix; s += v[i].CStr(); s += "\""; } s += "]"; };
            NkString s;
            s += "with project(\""; s += p.name; s += "\"):\n";
            s += "    "; s += (p.kind >= 0 && p.kind < 5 ? KINDS[p.kind] : "consoleapp"); s += "()\n";
            s += "    language(\""; s += (p.lang >= 0 && p.lang < 6 ? LANGS[p.lang] : "C++"); s += "\")\n";
            if (p.lang == 0) s += "    cdialect(\"C17\")\n";   // C
            else { s += "    cppdialect(\""; s += (p.dialect >= 0 && p.dialect < 5 ? DIALS[p.dialect] : "C++20"); s += "\")\n"; }
            // Fichier .jenga separe -> il est DEJA dans le dossier du projet : location relative = "."
            s += "    location(\""; s += (p.separateFile ? "." : p.location); s += "\")\n";
            // files/includedirs sont RELATIFS a location -> pas de prefixe (sinon double chemin = 0 source trouvee).
            s += "    files("; lst(s, p.sources, ""); s += ")\n";
            s += "    includedirs("; lst(s, p.includes, ""); s += ")\n";
            // Repertoires de sortie editables (objdir/targetdir/bindir). bindir omis si == targetdir (defaut).
            if (p.objDir[0]) { s += "    objdir(\""; s += p.objDir; s += "\")\n"; }
            if (p.tgtDir[0]) { s += "    targetdir(\""; s += p.tgtDir; s += "\")\n"; }
            if (p.binDir[0] && !StrEq(p.binDir, p.tgtDir)) { s += "    bindir(\""; s += p.binDir; s += "\")\n"; }
            if (!p.dependsOn.Empty()) { s += "    dependson("; lst(s, p.dependsOn, ""); s += ")\n"; }
            // filtres systeme : 1 bloc par filtre PRESENT (o.on). Vide -> `pass` (Python valide).
            const int32 nOs = NkWizOsCount();
            for (int32 i = 0; i < nOs; ++i) {
                const NkWizProject::OsFlt& o = p.os[i];
                if (!o.on) continue;
                const int32 sp = NkWizOsSpecial(i);
                const bool andOv = (sp == 1 && o.androidKind != 1);
                s += "\n    with filter(\""; s += NkWizOsFilt(i); s += "\"):\n";
                bool any = false;
                if (andOv) { s += "        "; s += (o.androidKind >= 0 && o.androidKind < 5 ? KINDS[o.androidKind] : "windowedapp"); s += "()\n"; any = true; }
                if (o.toolchain > 0) { s += "        usetoolchain(\""; s += TCS[o.toolchain]; s += "\")\n"; any = true; }
                if (sp == 2) { char mb[12]; std::snprintf(mb, sizeof(mb), "%d", (o.webMem >= 0 && o.webMem < 5) ? MEM[o.webMem] : 32); s += "        emscripteninitialmemory("; s += mb; s += ")\n"; any = true; }
                if (!o.links.Empty())   { s += "        links("; lst(s, o.links, ""); s += ")\n"; any = true; }
                if (!o.defines.Empty()) { s += "        defines("; lst(s, o.defines, ""); s += ")\n"; any = true; }
                if (!any) s += "        pass\n";
            }
            // filtres de configuration (un par config du workspace)
            for (usize c = 0; c < p.cfgFlt.Size(); ++c) {
                const NkWizProject::CfgFlt& cf = p.cfgFlt[c];
                s += "\n    with filter(\"config:"; s += cf.name.CStr(); s += "\"):\n";
                if (!cf.defines.Empty()) { s += "        defines("; lst(s, cf.defines, ""); s += ")\n"; }
                s += "        optimize(\""; s += (cf.opt >= 0 && cf.opt < 4 ? OPT[cf.opt] : "Off"); s += "\")\n";
                s += "        symbols("; s += (cf.sym ? "True" : "False"); s += ")\n";
            }
            // filtres personnalises (expression libre). Vide -> `pass`.
            for (usize c = 0; c < p.custFlt.Size(); ++c) {
                const NkWizProject::CustFlt& cf = p.custFlt[c];
                if (!cf.expr[0]) continue;
                s += "\n    with filter(\""; s += cf.expr; s += "\"):\n";
                bool any = false;
                if (cf.toolchain > 0)    { s += "        usetoolchain(\""; s += TCS[cf.toolchain]; s += "\")\n"; any = true; }
                if (!cf.defines.Empty()) { s += "        defines("; lst(s, cf.defines, ""); s += ")\n"; any = true; }
                if (!cf.links.Empty())   { s += "        links("; lst(s, cf.links, ""); s += ")\n"; any = true; }
                if (!any) s += "        pass\n";
            }
            // Assignation toolchain par plateforme (etape 3) : niveau PROJET (filtre respecte), toolchains Jenga valides.
            { const char* sysTok[7] = { "system:Windows", "system:Linux", "system:macOS", "system:Android", "system:iOS", "system:Web", "system:HarmonyOS" };
              const bool osOn[7] = { osWin, osLinux, osMac, osAndroid, osIos, osWeb, osHarmony };
              for (int32 pp = 0; pp < 7; ++pp) {
                  if (!osOn[pp] || tcPlatN[pp] <= 0) continue;
                  if (p.os[pp].on && p.os[pp].toolchain > 0) continue;   // le projet a deja son propre toolchain pour cette plateforme
                  const int32 sel = (tcPlatSel[pp] >= 0 && tcPlatSel[pp] < tcPlatN[pp]) ? tcPlatSel[pp] : 0;
                  const char* tcn = tcPlatPtrs[pp][sel];
                  if (!IsJengaTc(tcn)) continue;
                  s += "\n    with filter(\""; s += sysTok[pp]; s += "\"):\n        usetoolchain(\""; s += tcn; s += "\")\n";
              } }
            // Proprietes Android (app + workspace ciblant Android) : ces fonctions sont au niveau PROJET.
            if ((p.kind == 0 || p.kind == 1) && osAndroid) {
                static const char* API[] = { "21", "23", "24", "26", "28", "30", "31", "33", "34", "35" };
                auto api = [&](int32 i) -> const char* { return (i >= 0 && i < 10) ? API[i] : "24"; };
                s += "\n    with filter(\"system:Android\"):\n";
                s += "        androidapplicationid(\""; s += androidAppId; s += "\")\n";
                s += "        androidminsdk("; s += api(androidApiMin); s += ")\n";
                s += "        androidtargetsdk("; s += api(androidApiTarget); s += ")\n";
                s += "        androidcompilesdk("; s += api(androidCompileSdk); s += ")\n";
                s += "        androidabis([";
                { bool f = true; auto ab = [&](bool on, const char* a) { if (on) { if (!f) s += ", "; s += "\""; s += a; s += "\""; f = false; } };
                  ab(abiArm64v8a, "arm64-v8a"); ab(abiX86_64, "x86_64"); ab(abiArmeabi, "armeabi-v7a"); ab(abiX86, "x86"); }
                s += "])\n";
            }
            // Tests Unitest actives -> bloc « with test(): testfiles([...]) » IMBRIQUE dans le projet.
            if (p.genTest) {
                s += "\n    # Tests unitaires Unitest\n";
                if (p.testDesktopOnly) {
                    s += "    with filter(\"system:Windows || system:Linux || system:macOS\"):\n";
                    s += "        with test():\n";
                    s += "            testfiles([\""; s += p.testFiles; s += "\"])\n";
                } else {
                    s += "    with test():\n";
                    s += "        testfiles([\""; s += p.testFiles; s += "\"])\n";
                }
            }
            return s;
        }
        // Variante FICHIER SEPARE : imports principaux (fonctions Jenga + toolchains) + le projet.
        NkString BuildProjectFile(const NkWizProject& p) const {
            // En-tete (« guards ») Python : shebang + encodage + docstring, comme un .jenga standard.
            NkString s = "#!/usr/bin/env python3\n";
            s += "# -*- coding: utf-8 -*-\n";
            s += "\"\"\""; s += p.name; s += " - fichier projet Jenga (genere par NKCode).\"\"\"\n\n";
            s += "from Jenga import *\n";
            s += "from Jenga.GlobalToolchains import RegisterJengaGlobalToolchains\n\n";
            s += "RegisterJengaGlobalToolchains()\n\n";
            s += BuildProjectJenga(p);
            return s;
        }

        // Ecrit un fichier (cree les dossiers parents au besoin), chemin relatif au workspace.
        void WriteRel(const NkString& folder, const NkString& rel, const NkString& content) const {
            NkString full = folder; full += "/"; full += rel;
            const NkPath fp(full.CStr());
            NkDirectory::CreateRecursive(fp.GetParent().ToString().CStr());
            NkFile::WriteAllText(fp, content);
        }
        // Cree TOUT (dossiers + fichiers, workspace ET chaque projet), puis charge le workspace.
        bool Generate(NkCodeDialogs* dlg) {
            if (!Step1Valid()) return false;
            const NkString folder = FinalFolder();
            if (!NkDirectory::CreateRecursive(folder.CStr())) return false;
            // 1) Fichier workspace .jenga (edition manuelle respectee).
            const NkString wsContent = (wsExtractEdited && !wsExtractBuf.Empty()) ? wsExtractBuf : BuildJenga();
            if (!NkFile::WriteAllText((NkPath(folder.CStr()) / jengaFile), wsContent)) return false;
            // 2) Pour CHAQUE projet : dossiers + fichiers sources + headers + README + tests + .jenga separe.
            for (usize i = 0; i < projects.Size(); ++i) {
                const NkWizProject& p = projects[i];
                NkString loc = p.location; if (!loc.Empty() && loc.CStr()[loc.Size() - 1] != '/') loc += "/";   // normalise
                const bool isApp = (p.kind == 0 || p.kind == 1);
                const bool isLib = (p.kind == 2 || p.kind == 3);
                const char* lang = (p.lang == 0) ? "c" : "cpp";
                // .jenga separe
                if (p.separateFile) { NkString f = loc; f += p.name; f += ".jenga"; WriteRel(folder, f, BuildProjectFile(p)); }
                // main.cpp (apps)
                if (isApp && p.genMain) {
                    NkString c = "#include <iostream>\n\nint main()\n{\n    std::cout << \"Hello from "; c += p.name; c += " !\" << std::endl;\n    return 0;\n}\n";
                    NkString f = loc; f += "src/main."; f += lang; WriteRel(folder, f, c);
                }
                // header + source (libs)
                if (isLib) {
                    NkString h = "#pragma once\n\nnamespace "; h += p.name; h += "\n{\n    void Hello();\n}\n";
                    NkString hf = loc; hf += "include/"; hf += p.name; hf += ".h"; WriteRel(folder, hf, h);
                    NkString cpp = "#include \""; cpp += p.name; cpp += ".h\"\n#include <iostream>\n\nnamespace "; cpp += p.name; cpp += "\n{\n    void Hello()\n    {\n        std::cout << \""; cpp += p.name; cpp += " pret.\" << std::endl;\n    }\n}\n";
                    NkString cf = loc; cf += "src/"; cf += p.name; cf += ".cpp"; WriteRel(folder, cf, cpp);
                }
                // README
                if (p.genReadme) {
                    static const char* KN[] = { "consoleapp", "windowedapp", "staticlib", "sharedlib", "test" };
                    NkString r = "# "; r += p.name; r += "\n\nProjet `"; r += (p.kind >= 0 && p.kind < 5 ? KN[p.kind] : "consoleapp"); r += "` genere par NKCode.\n";
                    NkString f = loc; f += "README.md"; WriteRel(folder, f, r);
                }
                // tests Unitest
                if (p.genTest && p.kind != 4) {
                    NkString t = "#include <Unitest/Unitest.h>\n\nUTEST("; t += p.name; t += ", Basique)\n{\n    EXPECT_TRUE(true);\n}\n";
                    NkString f = loc; f += "tests/test_"; f += p.name; f += ".cpp"; WriteRel(folder, f, t);
                }
            }
            // 3) README workspace.
            { NkString r = "# "; r += wsName; r += "\n\nWorkspace Jenga genere par NKCode.\n\n## Build\n\n```\njenga build\n```\n";
              WriteRel(folder, NkString("README.md"), r); }
            // 4) .gitignore.
            if (gitIgnore) NkFile::WriteAllText((NkPath(folder.CStr()) / ".gitignore"),
                NkString("Build/\n.jenga/\n*.o\n*.obj\n*.exe\n*.dll\n*.so\n*.a\n*.lib\n"));
            // 5) Depot Git (init).
            if (gitInit) { NkString cmd = "git -C \""; cmd += folder; cmd += "\" init"; TcRun(cmd.CStr()); }
            if (dlg) dlg->DoLoad(NkPath(folder.CStr()));   // ouvre dans l'editeur
            return true;
        }
    };

    // ── Combo deroulant fonctionnel (le menu est rendu en fin de panneau, par-dessus). ──
    inline void NkWizCombo(const NkUi& u, const NkRect& r, int32 id, NkNewWsState* w,
                           const char* const* opts, int32 n, int32* sel, bool blockBg) {
        const bool open = w->comboOpen == id;
        u.Panel(r, NkCol::input, open ? NkCol::primary : NkCol::border, NkR::md * u.S);
        const int32 s = (*sel >= 0 && *sel < n) ? *sel : 0;
        u.TextEllipsis(r.x + u.s(10), r.y + (r.h - u.Lh()) * 0.5f, r.w - u.s(34), opts[s], NkCol::foreground);
        u.Icon("chevron-down", { r.x + r.w - u.s(18), r.y + (r.h - u.s(10)) * 0.5f, u.s(10), u.s(10) }, NkCol::mutedFg);
        if (!blockBg && u.Hit(r) && u.click) {
            if (open) w->comboOpen = -1;
            else { w->comboOpen = id; w->comboR = r; w->comboOpts = opts; w->comboN = n; w->comboSel = sel; w->comboJustOpened = true; }
        }
    }

    // ── Helpers de mise en page du wizard ──

    // Barre d'etapes : pastilles RONDES numerotees + connecteurs (fait=accent, actif=primary).
    // Restent rondes en tout etat (cercle plein, pas de bord carre).
    inline void NkWizSteps(const NkUi& u, const NkRect& r, int32 current, const char* const* labels, int32 count, uint32 doneTex = 0) {
        u.Rect(r, NkCol::sidebar);
        u.Rect({ r.x, r.y + r.h - 1.f, r.w, 1.f }, NkCol::border);
        const float32 dotD = u.s(30), lh = u.Lh();
        const float32 segW = u.s(54);
        const float32 cellW = u.s(94);
        const float32 totalW = count * cellW + (count - 1) * segW;
        float32 x = r.x + (r.w - totalW) * 0.5f; if (x < r.x + u.s(20)) x = r.x + u.s(20);
        // Bloc pastille + libelle CENTRE verticalement dans la barre.
        const float32 blockH = dotD + u.s(8) + lh;
        const float32 cy = r.y + (r.h - 1.f - blockH) * 0.5f;
        for (int32 i = 0; i < count; ++i) {
            const bool done = i < current, active = i == current;
            if (i > 0) {
                const float32 sx = x - segW;
                u.dl->AddRectFilled({ sx + u.s(2), cy + dotD * 0.5f - u.s(1.5f), segW - u.s(4), u.s(3) }, (done || active) ? NkCol::accent : NkCol::border, u.s(1.5f));
            }
            const NkRect dot = { x + (cellW - dotD) * 0.5f, cy, dotD, dotD };
            if (active) u.dl->AddRectFilled({ dot.x - u.s(3), dot.y - u.s(3), dotD + u.s(6), dotD + u.s(6) }, NkColor{ 15,115,213,60 }, (dotD + u.s(6)) * 0.5f);
            const NkColor bg = done ? NkCol::accent : (active ? NkCol::primary : NkCol::muted);
            u.dl->AddRectFilled(dot, bg, dotD * 0.5f);
            if (done) {                                    // etape validee -> icone de validation simple
                const NkRect ir = { dot.x + u.s(7), dot.y + u.s(7), dotD - u.s(14), dotD - u.s(14) };
                if (doneTex) NkDrawIcon(u, doneTex, ir, NkCol::primaryFg); else u.Icon("check", ir, NkCol::primaryFg);
            } else { char num[4]; std::snprintf(num, sizeof(num), "%d", i + 1);
                   const float32 tw = u.TextW(num);
                   u.Text(dot.x + (dotD - tw) * 0.5f, dot.y + (dotD - lh) * 0.5f, num, active ? NkCol::primaryFg : NkCol::mutedFg); }
            const float32 lw = u.TextW(labels[i]);
            u.Text(x + (cellW - lw) * 0.5f, cy + dotD + u.s(8), labels[i],
                   active ? NkCol::foreground : (done ? NkCol::accent : NkCol::mutedFg));
            x += cellW + segW;
        }
    }

    // Libelle de section (majuscules, espace lettres).
    inline void NkWizLabel(const NkUi& u, float32 x, float32 y, const char* t) {
        u.Text(x, y, t, NkCol::mutedFg);
    }
    // Ligne d'indice (fleche accent + texte) sous un champ -> code .jenga genere.
    inline void NkWizHint(const NkUi& u, float32 x, float32 y, float32 w, const char* t) {
        u.Icon("arrow-right", { x, y, u.s(14), u.s(14) }, NkCol::accent);
        u.TextEllipsis(x + u.s(20), y, w - u.s(20), t, NkCol::mutedFg);
    }
    // Case a cocher (style accent, arrondie/lisse) ; renvoie true si bascule.
    inline bool NkWizCheck(const NkUi& u, float32 x, float32 cy, const char* label, bool val, bool blockBg) {
        const float32 bs = u.s(18);                                   // un peu plus grande
        const NkRect b = { x, cy - bs * 0.5f, bs, bs };
        const float32 lw = u.TextW(label);
        const NkRect hit = { x - u.s(3), cy - bs * 0.5f - u.s(4), bs + u.s(10) + lw + u.s(10), bs + u.s(8) };
        const bool hv = u.Hit(hit);
        u.Panel(b, val ? NkCol::accent : NkCol::input, val ? NkCol::accent : NkCol::border, NkR::md * u.S);  // coins lisses
        if (val) { const float32 s = bs;
            u.dl->AddLine({ b.x + s * 0.26f, b.y + s * 0.52f }, { b.x + s * 0.44f, b.y + s * 0.70f }, NkCol::primaryFg, u.s(2.1f));
            u.dl->AddLine({ b.x + s * 0.44f, b.y + s * 0.70f }, { b.x + s * 0.76f, b.y + s * 0.30f }, NkCol::primaryFg, u.s(2.1f)); }
        u.Text(x + bs + u.s(10), cy - u.Lh() * 0.5f, label, hv ? NkCol::foreground : NkCol::sidebarFg);
        return hv && u.click && !blockBg;
    }

    // Champ texte avec focus + caret (reutilise NkOwEdit). leftPad reserve l'icone eventuelle.
    inline void NkWizField(const NkUi& u, const NkRect& r, char* buf, int32 cap, int32 id,
                           NkNewWsState* w, float32 dt, bool blockBg, float32 leftPad, bool readOnly = false) {
        const bool foc = (w->focus == id) && !readOnly;
        u.Panel(r, readOnly ? NkCol::muted : NkCol::input, foc ? NkCol::primary : NkCol::border, NkR::md * u.S);
        if (foc) NkOwEdit(u, r, buf, cap, w->caret, w->blink, dt, leftPad);
        else u.TextEllipsis(r.x + leftPad, r.y + (r.h - u.Lh()) * 0.5f, r.w - leftPad - u.s(8), buf, (readOnly ? NkCol::mutedFg : (buf[0] ? NkCol::foreground : NkCol::mutedFg)));
        if (!readOnly && !blockBg && u.Hit(r) && u.click) {
            // Positionne le caret SUR le caractere clique (sinon il filait toujours a la fin).
            int32 len = 0; while (buf[len]) ++len;
            const float32 viewW = r.w - leftPad - u.s(6);
            float32 off = 0.f;   // defilement horizontal courant (cf. NkOwEdit) si deja focus
            if (foc) { NkString pre; const int32 cc = (w->caret < len ? w->caret : len); for (int32 k = 0; k < cc; ++k) pre += buf[k]; const float32 pw = u.TextW(pre.CStr()); off = (pw > viewW) ? (pw - viewW) : 0.f; }
            const float32 target = u.mp.x - (r.x + leftPad) + off;
            int32 best = 0; float32 bestd = 1e9f; NkString acc;
            for (int32 i = 0; ; ++i) { const float32 wv = u.TextW(acc.CStr()); const float32 d = (wv > target) ? (wv - target) : (target - wv); if (d < bestd) { bestd = d; best = i; } if (!buf[i]) break; acc += buf[i]; }
            w->SetFocus(id, buf); w->caret = best; w->blink = 0.f; w->focusClaimed = true;
        }
    }

    // ── Pied de page : Precedent / Annuler / Suivant|Creer. Renvoie -1 prec, 0 rien, 1 annuler, 2 suivant. ──
    inline int32 NkWizFooter(const NkUi& u, const NkRect& r, int32 step, int32 lastStep, bool blockBg, bool nextEnabled,
                             const char* lastLabel = "Creer le Workspace", bool showCreateNow = true, uint32 validTex = 0) {
        u.Rect(r, NkCol::sidebar);
        u.Rect({ r.x, r.y, r.w, 1.f }, NkCol::border);
        int32 act = 0;
        const float32 bh = u.s(34), by = r.y + (r.h - bh) * 0.5f;
        float32 rx = r.x + r.w - u.s(20);
        // Icone du bouton de creation : validation si OK, rejet si invalide.
        auto createIcon = [&](float32 ix, float32 iy, const NkColor& fg) {
            if (nextEnabled) NkOwIco(u, validTex, "check", { ix, iy, u.s(14), u.s(14) }, fg);
            else             u.Icon("x", { ix, iy, u.s(14), u.s(14) }, NkCol::danger);
        };
        // Suivant / Creer (grise si etape invalide)
        const bool last = step == lastStep;
        const float32 nextW = last ? (u.TextW(lastLabel) + u.s(50)) : u.s(120);
        const NkRect nextR = { rx - nextW, by, nextW, bh }; rx -= nextW + u.s(8);
        { const bool hv = nextEnabled && !blockBg && u.Hit(nextR);
          const NkColor nbg = !nextEnabled ? NkCol::muted : (hv ? NkColHover(NkCol::primary) : NkCol::primary);
          const NkColor nfg = nextEnabled ? NkCol::primaryFg : NkCol::mutedFg;
          u.Rect(nextR, nbg, NkR::md * u.S);
          const char* lbl = last ? lastLabel : NkT("btn.next");
          const float32 tw = u.TextW(lbl);
          u.TextV(nextR.x + (nextW - tw - u.s(18)) * 0.5f, nextR.y, bh, lbl, nfg);
          const float32 ix = nextR.x + (nextW - tw - u.s(18)) * 0.5f + tw + u.s(6), iy = nextR.y + (bh - u.s(14)) * 0.5f;
          if (last) createIcon(ix, iy, nfg); else u.Icon("arrow-right", { ix, iy, u.s(13), u.s(13) }, nfg);
          if (hv && u.click) act = 2; }
        // Creer maintenant (sur les etapes NON finales : cree avec les valeurs par defaut)
        if (!last && showCreateNow) {
            const float32 crw = u.TextW(lastLabel) + u.s(50); const NkRect crR = { rx - crw, by, crw, bh }; rx -= crw + u.s(8);
            const bool hv = nextEnabled && !blockBg && u.Hit(crR);
            u.Rect(crR, !nextEnabled ? NkCol::muted : (hv ? NkColHover(NkCol::success) : NkCol::success), NkR::md * u.S);
            const float32 tw = u.TextW(lastLabel);
            const NkColor fg = nextEnabled ? NkCol::primaryFg : NkCol::mutedFg;
            createIcon(crR.x + (crw - tw - u.s(20)) * 0.5f, crR.y + (bh - u.s(14)) * 0.5f, fg);
            u.TextV(crR.x + (crw - tw - u.s(20)) * 0.5f + u.s(20), crR.y, bh, lastLabel, fg);
            if (hv && u.click) act = 3;
        }
        // Annuler
        const float32 cw = u.s(96); const NkRect cancelR = { rx - cw, by, cw, bh }; rx -= cw + u.s(8);
        if (u.Button(cancelR, NkT("btn.cancel"), NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S) && !blockBg) act = 1;
        // Precedent (a gauche)
        if (step > 0) {
            const NkRect prevR = { r.x + u.s(20), by, u.s(120), bh };
            const bool hv = !blockBg && u.Hit(prevR);
            u.Rect(prevR, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
            u.Icon("arrow-left", { prevR.x + u.s(14), prevR.y + (bh - u.s(13)) * 0.5f, u.s(13), u.s(13) }, NkCol::foreground);
            u.TextV(prevR.x + u.s(34), prevR.y, bh, NkT("btn.prev"), NkCol::foreground);
            if (hv && u.click) act = -1;
        }
        return act;
    }

    // Petit selecteur visuel (texte + chevron). Visuel uniquement pour l'instant.
    inline void NkWizSelect(const NkUi& u, const NkRect& r, const char* value) {
        u.Panel(r, NkCol::input, NkCol::border, NkR::md * u.S);
        u.TextV(r.x + u.s(10), r.y, r.h, value, NkCol::foreground);
        u.Icon("chevron-down", { r.x + r.w - u.s(18), r.y + (r.h - u.s(10)) * 0.5f, u.s(10), u.s(10) }, NkCol::mutedFg);
    }

    // ── ETAPE 1 : Informations du Workspace (2 colonnes). Renvoie la hauteur du contenu. ──
    inline float32 NkWizStep1(const NkUi& u, const NkRect& body, NkNewWsState* w, NkCodeDialogs* dlg, float32 dt, bool blockBg, const NkIcons& ic) {
        const float32 lh = u.Lh();
        // Colonnes : centre (champs principaux) + droite (Git + Nouvelle config).
        const float32 padL = u.s(28);
        const float32 rightW = (body.w > u.s(720)) ? u.s(290) : u.s(250);
        const float32 cx = body.x + padL;
        const float32 rx = body.x + body.w - u.s(28) - rightW;
        const float32 cw = (rx - u.s(34)) - cx;
        const float32 fH = u.s(32);

        const float32 secGap = u.s(38);     // espace GENEREUX entre sous-parties
        const float32 labGap = u.s(22);      // libelle -> champ
        const float32 hintGap = u.s(10);     // champ -> indice

        // ───────── COLONNE CENTRALE ─────────
        float32 y = body.y + u.s(22);
        // NOM DU WORKSPACE
        NkWizLabel(u, cx, y, NkT("nws.wsname")); y += labGap;
        NkWizField(u, { cx, y, cw, fH }, w->wsName, (int32)sizeof(w->wsName), 1, w, dt, blockBg, u.s(10)); y += fH + hintGap;
        { char hint[220]; std::snprintf(hint, sizeof(hint), "Correspond a workspace(\"%s\") dans le .jenga", w->wsName);
          NkWizHint(u, cx, y, cw, hint); } y += secGap;

        // EMPLACEMENT
        NkWizLabel(u, cx, y, NkT("nws.location")); y += labGap;
        const float32 browseW = u.s(110);
        NkWizField(u, { cx, y, cw - browseW - u.s(8), fH }, w->location, (int32)sizeof(w->location), 2, w, dt, blockBg, u.s(10));
        { const NkRect br = { cx + cw - browseW, y, browseW, fH };
          const bool hv = !blockBg && u.Hit(br);
          u.Rect(br, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
          NkOwIco(u, ic.ouvrirDossier, "folder-open", { br.x + u.s(14), br.y + (fH - u.s(14)) * 0.5f, u.s(14), u.s(14) }, NkCol::mutedFg);
          u.TextV(br.x + u.s(34), br.y, fH, NkT("btn.browse"), NkCol::foreground);
          if (hv && u.click && !blockBg && dlg) { w->focus = -1; dlg->BrowseInto(w->location, (int32)sizeof(w->location), "Emplacement du workspace"); } }
        y += fH + hintGap;
        { char fh[480]; const NkString ff = w->FinalFolder();
          std::snprintf(fh, sizeof(fh), "Dossier final : %s (cree s'il n'existe pas)", ff.CStr());
          NkWizHint(u, cx, y, cw, fh); } y += secGap;

        // NOM DU FICHIER .JENGA
        NkWizLabel(u, cx, y, NkT("nws.jengafile")); y += labGap;
        NkWizField(u, { cx, y, cw - u.s(96), fH }, w->jengaFile, (int32)sizeof(w->jengaFile), 3, w, dt, blockBg, u.s(10));
        u.TextV(cx + cw - u.s(90), y, fH, w->jengaManual ? "(manuel)" : "(auto-rempli)", NkCol::mutedFg);
        y += fH + secGap;

        // CONFIGURATIONS DE BUILD (Debug + Release par defaut ; l'utilisateur ajoute le reste)
        NkWizLabel(u, cx, y, NkT("nws.buildcfg")); y += labGap;
        const float32 cfgBoxH = u.s(80) + (w->customCfgs.Empty() ? 0.f : u.s(26) * ((w->customCfgs.Size() + 2) / 3));
        { const NkRect box = { cx, y, cw, cfgBoxH };
          u.Panel(box, NkCol::input, NkCol::border, NkR::md * u.S);
          float32 bx = box.x + u.s(16); const float32 bcy = box.y + u.s(24);
          if (NkWizCheck(u, bx, bcy, "Debug", w->cfgDebug, blockBg))     w->cfgDebug = !w->cfgDebug;     bx += u.s(100);
          if (NkWizCheck(u, bx, bcy, "Release", w->cfgRelease, blockBg)) w->cfgRelease = !w->cfgRelease; bx += u.s(108);
          for (usize i = 0; i < w->customCfgs.Size(); ++i) {           // config custom : clic = (de)cocher
              if (NkWizCheck(u, bx, bcy, w->customCfgs[i].name.CStr(), w->customCfgs[i].on, blockBg)) w->customCfgs[i].on = !w->customCfgs[i].on;
              bx += u.TextW(w->customCfgs[i].name.CStr()) + u.s(40);
          }
          // (l'ajout de configuration custom se fait via le formulaire « Nouvelle configuration » a droite)
          u.TextV(box.x + u.s(16), box.y + cfgBoxH - u.s(28), u.s(22), "Ajoutez-en via « Nouvelle configuration » -->", NkCol::mutedFg); }
        y += cfgBoxH + hintGap;
        { NkString cfgs; auto add = [&](bool on, const char* n) { if (on) { if (!cfgs.Empty()) cfgs += "\", \""; cfgs += n; } };
          add(w->cfgDebug, "Debug"); add(w->cfgRelease, "Release");
          for (usize i = 0; i < w->customCfgs.Size(); ++i) add(w->customCfgs[i].on, w->customCfgs[i].name.CStr());
          char hint[280]; std::snprintf(hint, sizeof(hint), "Correspond a configurations([\"%s\"]) dans le .jenga", cfgs.CStr());
          NkWizHint(u, cx, y, cw, hint); } y += secGap;

        // PROJET DE DEMARRAGE
        NkWizLabel(u, cx, y, NkT("nws.startproj")); y += labGap;
        { const NkRect pr = { cx, y, cw, fH };
          u.Panel(pr, NkCol::muted, NkCol::border, NkR::md * u.S);
          const char* txt = (w->startProj >= 0 && w->startProj < (int32)w->projects.Size())
              ? w->projects[w->startProj].name : "(sera defini apres creation des projets)";
          u.TextEllipsis(pr.x + u.s(10), pr.y + (fH - lh) * 0.5f, cw - u.s(18), txt, NkCol::mutedFg); }

        // ───────── COLONNE DROITE ─────────
        float32 ry = body.y + u.s(22);
        // GIT (dans un bloc)
        NkWizLabel(u, rx, ry, NkT("nws.git")); ry += labGap;
        { const NkRect box = { rx, ry, rightW, u.s(108) };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          float32 gy = box.y + u.s(20);
          if (NkWizCheck(u, box.x + u.s(14), gy, NkT("nws.gitinit"), w->gitInit, blockBg)) w->gitInit = !w->gitInit; gy += u.s(30);
          if (NkWizCheck(u, box.x + u.s(14), gy, NkT("nws.gitignore"), w->gitIgnore, blockBg)) w->gitIgnore = !w->gitIgnore; gy += u.s(30);
          if (NkWizCheck(u, box.x + u.s(14), gy, NkT("nws.gitcommit"), w->gitCommit, blockBg)) w->gitCommit = !w->gitCommit; }
        ry += u.s(108) + secGap;

        // NOUVELLE CONFIGURATION (formulaire fonctionnel : ajoute a CONFIGURATIONS)
        NkWizLabel(u, rx, ry, NkT("nws.newcfg")); ry += labGap;
        { const NkRect box = { rx, ry, rightW, u.s(188) };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          float32 by = box.y + u.s(14);
          u.Text(box.x + u.s(14), by, NkT("ow.col.name"), NkCol::mutedFg); by += u.s(20);
          NkWizField(u, { box.x + u.s(14), by, rightW - u.s(28), fH }, w->newCfgName, (int32)sizeof(w->newCfgName), 10, w, dt, blockBg, u.s(10));
          by += fH + u.s(14);
          u.Text(box.x + u.s(14), by, NkT("nws.basedon"), NkCol::mutedFg); by += u.s(20);
          { static const char* baseOpts[] = { "Debug", "Release" };
            NkWizCombo(u, { box.x + u.s(14), by, rightW - u.s(28), fH }, 20, w, baseOpts, 2, &w->newCfgBase, blockBg); }
          by += fH + u.s(16);
          const float32 bw = u.s(86), bh = u.s(30);
          if (u.Button({ box.x + rightW - u.s(14) - bw * 2 - u.s(8), by, bw, bh }, NkT("btn.cancel"), NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S) && !blockBg)
              { w->newCfgName[0] = '\0'; if (w->focus == 10) w->focus = -1; }
          { const NkRect ar = { box.x + rightW - u.s(14) - bw, by, bw, bh };
            const bool ahv = !blockBg && u.Hit(ar);
            u.Rect(ar, ahv ? NkColHover(NkCol::primary) : NkCol::primary, NkR::md * u.S);
            u.TextV(ar.x + (bw - u.TextW(NkT("btn.add"))) * 0.5f, ar.y, bh, NkT("btn.add"), NkCol::primaryFg);
            if (ahv && u.click) { w->AddCustomCfg(); } }
          ry = box.y + box.h; }

        // hauteur du contenu (max des deux colonnes) + marge basse
        const float32 contentH = ((y > ry ? y : ry) - body.y) + u.s(24);
        return contentH;
    }

    // Tables d'options partagees (cartes type + combos).
    inline const char* const* NkWizKinds(int32* n)    { static const char* k[] = { "consoleapp", "windowedapp", "staticlib", "sharedlib", "test" }; if (n) *n = 5; return k; }
    inline const char* const* NkWizLangs(int32* n)    { static const char* k[] = { "C", "C++", "ObjC", "ObjC++", "Zig", "Rust" }; if (n) *n = 6; return k; }
    inline const char* const* NkWizDialects(int32* n) { static const char* k[] = { "C++11", "C++14", "C++17", "C++20", "C++23" }; if (n) *n = 5; return k; }
    inline const char* NkWizKindIcon(int32 k) { const char* ic_[] = { "terminal", "app-window", "archive", "link", "flask" }; return (k >= 0 && k < 5) ? ic_[k] : "file"; }
    inline uint32 NkWizKindTex(const NkIcons& ic, int32 k) {
        switch (k) { case 0: return ic.kConsole; case 1: return ic.kWindowed; case 2: return ic.kStatic; case 3: return ic.kShared; case 4: return ic.kTest; }
        return 0;
    }
    inline bool NkWizOsEnabled(const NkNewWsState* w, int32 i) {
        switch (i) { case 0: return w->osWin; case 1: return w->osLinux; case 2: return w->osMac; case 3: return w->osAndroid; case 4: return w->osIos; case 5: return w->osWeb; case 6: return w->osHarmony; } return false;
    }
    // Toolchain (index NkWizTcs) -> OS qu'elle cible (index plateforme), ou -1 si generique.
    inline int32 NkWizTcOs(int32 tc) { switch (tc) { case 1: return 0; case 6: return 0; case 2: return 1; case 5: return 1; case 3: return 3; case 4: return 5; } return -1; }

    // ── ETAPE 2 : Projets du workspace. Renvoie la hauteur du contenu. ──
    inline float32 NkWizStep2(const NkUi& u, const NkRect& body, NkNewWsState* w, float32 dt, bool blockBg, const NkIcons& ic) {
        (void)dt; (void)ic;
        const float32 lh = u.Lh();
        const float32 x = body.x + u.s(28), wkW = body.w - u.s(56);
        float32 y = body.y + u.s(22);

        // En-tete + bouton « Ajouter un projet »
        NkWizLabel(u, x, y + u.s(4), NkT("nws.projects"));
        { const float32 aw = u.s(160); const NkRect addR = { x + wkW - aw, y, aw, u.s(30) };
          const bool hv = !blockBg && u.Hit(addR);
          u.Rect(addR, hv ? NkColHover(NkCol::primary) : NkCol::primary, NkR::md * u.S);
          const float32 tw = u.TextW(NkT("nws.addproj"));
          NkOwIco(u, ic.plus, "plus", { addR.x + (aw - tw - u.s(20)) * 0.5f, addR.y + (u.s(30) - u.s(13)) * 0.5f, u.s(13), u.s(13) }, NkCol::primaryFg);
          u.TextV(addR.x + (aw - tw - u.s(20)) * 0.5f + u.s(20), addR.y, u.s(30), NkT("nws.addproj"), NkCol::primaryFg);
          if (hv && u.click) w->OpenProjDlg(-1); }
        y += u.s(40);

        if (w->projects.Empty()) {
            const NkRect box = { x, y, wkW, u.s(96) };
            u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
            const char* m1 = NkT("nws.noproj");
            const char* m2 = NkT("nws.noproj2");
            u.Text(box.x + (box.w - u.TextW(m1)) * 0.5f, box.y + u.s(30), m1, NkCol::foreground);
            u.Text(box.x + (box.w - u.TextW(m2)) * 0.5f, box.y + u.s(52), m2, NkCol::mutedFg);
            y += u.s(106);
        } else {
            int32 doEdit = -1, doDel = -1, doStar = -1;
            for (usize i = 0; i < w->projects.Size(); ++i) {
                const NkWizProject& p = w->projects[i];
                const float32 ch = u.s(96);
                const NkRect card = { x, y, wkW, ch };
                u.Panel(card, NkCol::surface, NkCol::border, NkR::md * u.S);
                // icone du TYPE d'application + nom
                NkOwIco(u, NkWizKindTex(ic, p.kind), NkWizKindIcon(p.kind), { card.x + u.s(14), card.y + u.s(14), u.s(18), u.s(18) }, NkCol::accent);
                u.Text(card.x + u.s(40), card.y + u.s(15), p.name, NkCol::foreground);
                // boutons droite : Star / Editer / Supprimer
                float32 bx = card.x + card.w - u.s(14);
                { const float32 tw = u.s(28); const NkRect r = { bx - tw, card.y + u.s(12), tw, u.s(24) }; bx -= tw + u.s(6);
                  const bool hv = !blockBg && u.Hit(r);
                  u.Rect(r, hv ? NkCol::hover : NkCol::muted, NkR::sm * u.S);
                  NkOwIco(u, ic.corbeille, "trash", { r.x + u.s(7), r.y + u.s(5), u.s(14), u.s(14) }, NkCol::danger);
                  if (hv && u.click) doDel = (int32)i; }
                { const float32 tw = u.s(28); const NkRect r = { bx - tw, card.y + u.s(12), tw, u.s(24) }; bx -= tw + u.s(6);
                  const bool hv = !blockBg && u.Hit(r);
                  u.Rect(r, hv ? NkCol::hover : NkCol::muted, NkR::sm * u.S);
                  NkOwIco(u, ic.editer, "pencil", { r.x + u.s(7), r.y + u.s(5), u.s(14), u.s(14) }, NkCol::foreground);
                  if (hv && u.click) doEdit = (int32)i; }
                { const bool isStart = (w->startProj == (int32)i);
                  const float32 tw = u.s(70); const NkRect r = { bx - tw, card.y + u.s(12), tw, u.s(24) }; bx -= tw + u.s(6);
                  const bool hv = !blockBg && u.Hit(r);
                  u.Panel(r, isStart ? NkCol::secondary : (hv ? NkCol::hover : NkCol::muted), isStart ? NkCol::accent : NkCol::border, NkR::sm * u.S);
                  NkOwIco(u, ic.star, "star", { r.x + u.s(8), r.y + u.s(6), u.s(12), u.s(12) }, isStart ? NkCol::accent : NkCol::mutedFg);
                  u.TextV(r.x + u.s(24), r.y, u.s(24), NkT("nws.start"), isStart ? NkCol::accent : NkCol::mutedFg);
                  if (hv && u.click) doStar = (int32)i; }
                // ligne meta : type pill + langage/dialecte/dossier
                int32 nK = 0, nL = 0, nD = 0; const char* const* kinds = NkWizKinds(&nK); const char* const* langs = NkWizLangs(&nL); const char* const* dials = NkWizDialects(&nD);
                const char* kind = (p.kind >= 0 && p.kind < nK) ? kinds[p.kind] : "consoleapp";
                const bool isApp = (p.kind == 0 || p.kind == 1);
                { const float32 pw = u.TextW(kind) + u.s(16); const NkRect pill = { card.x + u.s(40), card.y + u.s(40), pw, u.s(18) };
                  u.Panel(pill, isApp ? NkCol::secondary : NkCol::muted, isApp ? NkCol::accent : NkCol::border, NkR::sm * u.S);
                  u.TextV(pill.x + u.s(8), pill.y - u.s(3), u.s(18), kind, isApp ? NkCol::accent : NkCol::mutedFg);
                  float32 mx = pill.x + pw + u.s(14); const float32 my = card.y + u.s(41);
                  char meta[200]; std::snprintf(meta, sizeof(meta), "Langage: %s  ·  Dialecte: %s  ·  Dossier: %s",
                      (p.lang >= 0 && p.lang < nL) ? langs[p.lang] : "C++", (p.dialect >= 0 && p.dialect < nD) ? dials[p.dialect] : "C++20", p.location);
                  u.Text(mx, my, meta, NkCol::mutedFg); }
                // ligne sources/includes/depend
                { NkString line = NkT("nws.sourcesprefix");
                  for (usize k = 0; k < p.sources.Size(); ++k) { if (k) line += ", "; line += p.sources[k].CStr(); }
                  line += "   Includes: ";
                  for (usize k = 0; k < p.includes.Size(); ++k) { if (k) line += ", "; line += p.includes[k].CStr(); }
                  if (!p.dependsOn.Empty()) { line += "   Depend de: "; for (usize k = 0; k < p.dependsOn.Size(); ++k) { if (k) line += ", "; line += p.dependsOn[k].CStr(); } }
                  u.TextEllipsis(card.x + u.s(40), card.y + u.s(64), card.w - u.s(54), line.CStr(), NkCol::mutedFg); }
                y += ch + u.s(12);
            }
            if (doStar >= 0) w->startProj = (w->startProj == doStar) ? -1 : doStar;
            if (doEdit >= 0) w->OpenProjDlg(doEdit);
            if (doDel >= 0)  { w->projects.Erase(w->projects.Begin() + doDel); if (w->startProj == doDel) w->startProj = -1; else if (w->startProj > doDel) w->startProj--; }

            // ORDRE DE BUILD
            y += u.s(10);
            NkWizLabel(u, x, y, NkT("nws.buildorder")); u.Text(x + u.TextW(NkT("nws.buildorder")) + u.s(8), y, NkT("nws.buildorderhint"), NkCol::mutedFg);
            y += u.s(24);
            float32 px = x;
            for (usize i = 0; i < w->projects.Size(); ++i) {
                const NkWizProject& p = w->projects[i];
                const float32 pw = u.TextW(p.name) + u.s(34);
                const NkRect pill = { px, y, pw, u.s(30) };
                const bool hv = !blockBg && u.Hit(pill);
                u.Panel(pill, (w->dragProj == (int32)i) ? NkCol::secondary : (hv ? NkCol::hover : NkCol::muted), NkCol::border, NkR::md * u.S);
                NkOwIco(u, NkWizKindTex(ic, p.kind), NkWizKindIcon(p.kind), { pill.x + u.s(8), pill.y + u.s(8), u.s(14), u.s(14) }, NkCol::accent);
                u.TextV(pill.x + u.s(26), pill.y, u.s(30), p.name, NkCol::foreground);
                // drag pour reordonner
                if (hv && u.click) w->dragProj = (int32)i;
                px += pw;
                if (i + 1 < w->projects.Size()) {
                    // Fleche ACCENT seulement si le projet suivant depend reellement de celui-ci ; sinon gris (simple ordre).
                    const NkWizProject& nxt = w->projects[i + 1];
                    bool linked = false; for (usize k = 0; k < nxt.dependsOn.Size(); ++k) if (StrEq(nxt.dependsOn[k].CStr(), p.name)) { linked = true; break; }
                    u.Icon("arrow-right", { px + u.s(6), y + u.s(9), u.s(12), u.s(12) }, linked ? NkCol::accent : NkCol::muted);
                    px += u.s(24);
                }
            }
            // relachement du drag : echange avec la pilule survolee
            if (w->dragProj >= 0) {
                if (!u.down) {
                    float32 qx = x;
                    for (usize i = 0; i < w->projects.Size(); ++i) {
                        const float32 pw = u.TextW(w->projects[i].name) + u.s(34);
                        const NkRect pill = { qx, y, pw, u.s(30) };
                        if (u.Hit(pill) && (int32)i != w->dragProj) {
                            NkWizProject t = w->projects[w->dragProj]; w->projects.Erase(w->projects.Begin() + w->dragProj);
                            w->projects.Insert(w->projects.Begin() + i, t); break;
                        }
                        qx += pw + u.s(24);
                    }
                    w->dragProj = -1;
                }
            }
            y += u.s(40);
            NkWizHint(u, x, y, wkW, "Correspond a l'ordre de declaration with project(\"...\") dans le .jenga"); y += u.s(20);
        }
        return (y - body.y) + u.s(24);
    }

    // Petite liste editable (entrees + bouton x) + champ d'ajout. Renvoie la hauteur utilisee.
    inline float32 NkWizEditList(const NkUi& u, float32 x, float32 y, float32 wdt, NkVector<NkString>& items,
                                 char* addBuf, int32 addCap, int32 fieldId, NkNewWsState* w, float32 dt, bool blockBg, uint32 plusTex = 0) {
        const float32 fH = u.s(28);
        const float32 listH = u.s(8) + (items.Empty() ? u.s(22) : items.Size() * u.s(24)) + u.s(4);
        const NkRect box = { x, y, wdt, listH };
        u.Panel(box, NkCol::input, NkCol::border, NkR::md * u.S);
        int32 rm = -1;
        if (items.Empty()) u.TextV(box.x + u.s(10), box.y, u.s(28), "(vide)", NkCol::mutedFg);
        for (usize i = 0; i < items.Size(); ++i) {
            const float32 ry = box.y + u.s(6) + i * u.s(24);
            u.Text(box.x + u.s(10), ry, items[i].CStr(), NkCol::foreground);
            const NkRect xr = { box.x + wdt - u.s(24), ry - u.s(2), u.s(18), u.s(18) };
            const bool hv = !blockBg && u.Hit(xr);
            u.Icon("x", { xr.x + u.s(4), xr.y + u.s(4), u.s(10), u.s(10) }, hv ? NkCol::danger : NkCol::mutedFg);
            if (hv && u.click) rm = (int32)i;
        }
        if (rm >= 0) items.Erase(items.Begin() + rm);
        y += listH + u.s(6);
        // champ d'ajout + bouton
        const float32 addBtnW = u.s(90);
        NkWizField(u, { x, y, wdt - addBtnW - u.s(8), fH }, addBuf, addCap, fieldId, w, dt, blockBg, u.s(10));
        { const NkRect br = { x + wdt - addBtnW, y, addBtnW, fH };
          const bool hv = !blockBg && u.Hit(br);
          u.Rect(br, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
          NkOwIco(u, plusTex, "plus", { br.x + u.s(14), br.y + (fH - u.s(12)) * 0.5f, u.s(12), u.s(12) }, NkCol::foreground);
          u.TextV(br.x + u.s(30), br.y, fH, NkT("btn.add"), NkCol::foreground);
          if (hv && u.click && addBuf[0]) { items.PushBack(NkString(addBuf)); addBuf[0] = '\0'; if (w->focus == fieldId) w->focus = -1; } }
        return (y + fH) - (box.y);
    }

    // ── Dialog « Ajouter / Editer un projet » (modal scrollable). ──
    // Options des combos du projet (toolchains, optimize, memoire web).
    inline const char* const* NkWizTcs(int32* n)  { static const char* k[] = { "(auto)", "clang-mingw", "zig-linux-x64", "android-ndk", "emscripten", "clang-native", "clang-cl" }; if (n) *n = 7; return k; }
    inline const char* const* NkWizOpt(int32* n)  { static const char* k[] = { "Off", "Size", "Speed", "Full" }; if (n) *n = 4; return k; }
    inline const char* const* NkWizMem(int32* n)  { static const char* k[] = { "16 MB", "32 MB", "64 MB", "128 MB", "256 MB" }; if (n) *n = 5; return k; }
    inline int32 NkWizMemMB(int32 i) { const int32 m[] = { 16, 32, 64, 128, 256 }; return (i >= 0 && i < 5) ? m[i] : 32; }

    // Liste de « pilules » editables (links/defines) : pills + bouton « + Ajouter » (champ partage).
    inline float32 NkProjPills(const NkUi& u, float32 x, float32 y, float32 maxW, NkVector<NkString>& items,
                               int32 listId, NkNewWsState* w, float32 dt, bool blockBg, uint32 plusTex = 0) {
        const float32 ph = u.s(22); float32 cx = x; int32 rm = -1;
        for (usize i = 0; i < items.Size(); ++i) {
            const float32 pw = u.TextW(items[i].CStr()) + u.s(26);
            if (cx + pw > x + maxW) { cx = x; y += ph + u.s(6); }
            const NkRect pill = { cx, y, pw, ph };
            u.Panel(pill, NkCol::muted, NkCol::border, NkR::sm * u.S);
            u.TextV(pill.x + u.s(8), pill.y - u.s(3), ph, items[i].CStr(), NkCol::foreground);
            const NkRect xr = { pill.x + pw - u.s(16), pill.y + u.s(4), u.s(13), u.s(13) };
            const bool hv = !blockBg && u.Hit(xr);
            u.Icon("x", { xr.x + u.s(2), xr.y + u.s(2), u.s(9), u.s(9) }, hv ? NkCol::danger : NkCol::mutedFg);
            if (hv && u.click) rm = (int32)i;
            cx += pw + u.s(6);
        }
        if (rm >= 0) items.Erase(items.Begin() + rm);
        // champ d'ajout (inline si cette liste est la cible) ou bouton « + Ajouter »
        if (w->projFltTarget == listId) {
            const float32 fw = u.s(120);
            if (cx + fw + u.s(70) > x + maxW) { cx = x; y += ph + u.s(6); }
            NkWizField(u, { cx, y - u.s(4), fw, u.s(26) }, w->projFltAdd, (int32)sizeof(w->projFltAdd), 200, w, dt, blockBg, u.s(8));
            const NkRect ok = { cx + fw + u.s(6), y - u.s(4), u.s(60), u.s(26) };
            const bool hv = !blockBg && u.Hit(ok);
            u.Rect(ok, hv ? NkColHover(NkCol::primary) : NkCol::primary, NkR::sm * u.S);
            u.TextV(ok.x + u.s(10), ok.y, u.s(26), "OK", NkCol::primaryFg);
            const bool commit = (hv && u.click) || u.ctx->input.KeyPressed(NkGuiKey::Enter);
            if (commit) { if (w->projFltAdd[0]) items.PushBack(NkString(w->projFltAdd)); w->projFltAdd[0] = '\0'; w->projFltTarget = -1; w->focus = -1; }
        } else {
            if (cx + u.s(96) > x + maxW) { cx = x; y += ph + u.s(6); }   // bouton « Ajouter » : retour a la ligne s'il deborde
            const NkRect ar = { cx, y - u.s(3), u.s(90), ph + u.s(4) };
            const bool hv = !blockBg && u.Hit(ar);
            NkOwIco(u, plusTex, "plus", { ar.x + u.s(4), ar.y + u.s(7), u.s(11), u.s(11) }, hv ? NkCol::foreground : NkCol::mutedFg);
            u.TextV(ar.x + u.s(18), ar.y, ph + u.s(4), NkT("btn.add"), hv ? NkCol::foreground : NkCol::mutedFg);
            if (hv && u.click) { w->projFltTarget = listId; w->projFltAdd[0] = '\0'; w->SetFocus(200, w->projFltAdd); }
        }
        return y + ph + u.s(8);
    }

    // Hauteur d'une liste de pills (wrap) pour dimensionner les boites (auto-grow).
    inline float32 NkProjPillsH(const NkUi& u, float32 maxW, const NkVector<NkString>& items, bool addActive) {
        const float32 ph = u.s(22); float32 cx = 0.f; int32 lines = 1;
        for (usize i = 0; i < items.Size(); ++i) { const float32 pw = u.TextW(items[i].CStr()) + u.s(26); if (cx + pw > maxW) { cx = 0.f; ++lines; } cx += pw + u.s(6); }
        const float32 addW = addActive ? u.s(196) : u.s(96);
        if (cx + addW > maxW) ++lines;
        return lines * (ph + u.s(6)) + u.s(2);
    }

    // ── Coloration syntaxique d'UNE ligne .jenga (token par token, indentation preservee). ──
    inline void NkCodeDrawLine(const NkUi& u, float32 x, float32 y, const NkString& line) {
        static const char* KW[] = { "with", "from", "import", "as", "True", "False", "None", "and", "or", "not", "if", "else", "def", "return" };
        const NkColor cDef = NkCol::foreground, cKw = NkColor{ 197,134,192,255 }, cFn = NkColor{ 86,156,214,255 },
                      cStr = NkColor{ 206,145,120,255 }, cNum = NkColor{ 181,206,168,255 }, cCom = NkColor{ 106,153,85,255 };
        const char* s = line.CStr(); const usize n = line.Size(); float32 cx = x;
        auto emit = [&](const NkString& seg, const NkColor& col) { if (seg.Empty()) return; u.Text(cx, y, seg.CStr(), col); cx += u.TextW(seg.CStr()); };
        auto isAl = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
        auto isAln = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; };
        usize i = 0;
        while (i < n) {
            const char c = s[i];
            if (c == '#') { NkString seg; while (i < n) { seg += s[i]; ++i; } emit(seg, cCom); break; }
            if (c == '"' || c == '\'') { const char q = c; NkString seg; seg += s[i]; ++i; while (i < n) { seg += s[i]; if (s[i] == q) { ++i; break; } ++i; } emit(seg, cStr); continue; }
            if (isAl(c)) { NkString seg; usize j = i; while (j < n && isAln(s[j])) { seg += s[j]; ++j; }
                usize k = j; while (k < n && s[k] == ' ') ++k; const bool fn = (k < n && s[k] == '(');
                bool kw = false; for (const char* kw2 : KW) if (seg == kw2) { kw = true; break; }
                emit(seg, kw ? cKw : (fn ? cFn : cDef)); i = j; continue; }
            if (c >= '0' && c <= '9') { NkString seg; while (i < n && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.')) { seg += s[i]; ++i; } emit(seg, cNum); continue; }
            NkString seg; seg += c; ++i; emit(seg, cDef);
        }
    }

    // ── Visualiseur de code : hauteur FIXE, scroll vertical + horizontal, numeros, coloration. ──
    //    Partage w->codeSX / w->codeSY (un seul visible a la fois entre apercu et extrait).
    // Selection PARTAGEE de l'editeur de code multi-ligne (un seul champ edite a la fois) :
    // l'ancre est posee par NkWizCodeEditInput, lue par NkWizCodeBox pour la surbrillance.
    struct NkCodeSelSt { const void* owner = nullptr; int32 anchor = -1; };
    inline NkCodeSelSt& NkCodeSel() { static NkCodeSelSt s; return s; }

    inline void NkWizCodeBox(const NkUi& u, const NkRect& rect, const NkString& code, NkNewWsState* w, bool blockBg, bool lineNums, int32* caretEdit = nullptr, float32 blink = 0.f, bool followCaret = false) {
        const int32 caretPos = caretEdit ? *caretEdit : -1;
        bool caretMoved = followCaret;   // l'auto-scroll ne suit le caret QUE s'il vient de bouger (sinon la molette est annulee)
        u.Panel(rect, NkCol::input, (caretPos >= 0) ? NkCol::primary : NkCol::border, NkR::md * u.S);
        const float32 lh = u.Lh(), rowH = lh + u.s(6);   // interligne aere (lisibilite des blocs)
        const float32 gutter = lineNums ? u.s(40) : u.s(10);
        const float32 sbW = u.s(10);
        // Clic dans la zone (mode edition) -> positionne le caret sur le caractere vise.
        if (caretEdit && !blockBg && u.Hit(rect) && u.click) {
            int32 line = (int32)((u.mp.y - (rect.y + u.s(6)) + w->codeSY) / rowH); if (line < 0) line = 0;
            const char* s = code.CStr(); int32 gi = 0, cur = 0;
            while (cur < line && s[gi]) { if (s[gi] == '\n') ++cur; ++gi; }
            const float32 px = u.mp.x - (rect.x + gutter) + w->codeSX;
            int32 col = 0; float32 bestd = 1e9f; NkString acc;
            for (int32 k = 0; ; ++k) { const char ch = s[gi + k]; const float32 wv = u.TextW(acc.CStr()); const float32 d = (wv > px) ? (wv - px) : (px - wv); if (d < bestd) { bestd = d; col = k; } if (ch == '\0' || ch == '\n') break; acc += ch; }
            *caretEdit = gi + col; w->blink = 0.f; w->focusClaimed = true; caretMoved = true;
        }
        int32 nLines = 1; for (const char* s = code.CStr(); *s; ++s) if (*s == '\n') ++nLines;
        float32 maxW = 0.f; { NkString ln; for (const char* s = code.CStr(); ; ++s) { if (*s == '\n' || *s == '\0') { const float32 wv = u.TextW(ln.CStr()); if (wv > maxW) maxW = wv; ln.Clear(); if (*s == '\0') break; } else if (*s != '\r') ln += *s; } }
        const float32 contentH = nLines * rowH + u.s(8);
        const float32 viewH = rect.h, viewW = rect.w - gutter - sbW - u.s(6);
        const float32 maxSY = (contentH > viewH) ? contentH - viewH : 0.f;
        const float32 maxSX = (maxW > viewW) ? maxW - viewW + u.s(8) : 0.f;
        if (!blockBg && u.Hit(rect) && u.ctx->input.wheel != 0.f) {
            if (u.ctx->input.shiftDown) w->codeSX -= u.ctx->input.wheel * u.s(40);
            else                        w->codeSY -= u.ctx->input.wheel * u.s(40);
            u.ctx->input.wheel = 0.f;
        }
        // Position 2D du caret (mode edition). L'auto-scroll ne s'applique QUE si le caret vient de bouger
        // (sinon il ramenerait sans cesse la vue sur le caret et la molette ne pourrait pas atteindre le bas).
        int32 caretLine = 0; float32 caretX = 0.f;
        if (caretPos >= 0) {
            int32 cc = 0; { const char* s = code.CStr(); for (int32 k = 0; k < caretPos && s[k]; ++k) { if (s[k] == '\n') { ++caretLine; cc = 0; } else ++cc; } }
            NkString seg; { const char* s = code.CStr(); const int32 ls = caretPos - cc; for (int32 k = 0; k < cc; ++k) seg += s[ls + k]; }
            caretX = u.TextW(seg.CStr());
            if (caretMoved) {
                const float32 cy = caretLine * rowH;
                if (cy - w->codeSY < 0.f) w->codeSY = cy;
                if (cy + rowH - w->codeSY > viewH - u.s(10)) w->codeSY = cy + rowH - viewH + u.s(10);
                if (caretX - w->codeSX < 0.f) w->codeSX = caretX;
                if (caretX + u.s(10) - w->codeSX > viewW) w->codeSX = caretX + u.s(10) - viewW;
            }
        }
        if (w->codeSY < 0.f) w->codeSY = 0.f; if (w->codeSY > maxSY) w->codeSY = maxSY;
        if (w->codeSX < 0.f) w->codeSX = 0.f; if (w->codeSX > maxSX) w->codeSX = maxSX;
        const NkRect clip = { rect.x + u.s(2), rect.y + u.s(2), rect.w - u.s(4), rect.h - u.s(4) };
        u.dl->PushClipRect(clip, true);
        float32 ly = rect.y + u.s(6) - w->codeSY; int32 ln = 1; NkString line;
        for (const char* s = code.CStr(); ; ++s) {
            if (*s == '\n' || *s == '\0') {
                if (ly + rowH >= rect.y && ly <= rect.y + rect.h) {
                    if (lineNums) { char num[8]; std::snprintf(num, sizeof(num), "%d", ln); u.Text(rect.x + u.s(8), ly, num, NkCol::mutedFg); }
                    NkCodeDrawLine(u, rect.x + gutter - w->codeSX, ly, line);
                }
                ly += rowH; ++ln; line.Clear(); if (*s == '\0') break;
            } else if (*s != '\r') line += *s;
        }
        // Caret clignotant.
        if (caretPos >= 0 && blink < 0.5f) {
            const float32 px = rect.x + gutter + caretX - w->codeSX;
            const float32 py = rect.y + u.s(6) + caretLine * rowH - w->codeSY;
            if (px >= rect.x + gutter - u.s(2) && px <= rect.x + rect.w - sbW) u.dl->AddRectFilled({ px, py, u.s(1.5f), lh }, NkCol::foreground, 0.f);
        }
        u.dl->PopClipRect();
        if (lineNums) u.dl->AddRectFilled({ rect.x + gutter - u.s(5), rect.y + u.s(2), 1.f, rect.h - u.s(4) }, NkCol::border, 0.f);
        if (!u.down) { if (w->codeBar == 1 || w->codeBar == 2) w->codeBar = 0; }   // relache le drag
        if (maxSY > 0.5f) {
            const NkRect track = { rect.x + rect.w - sbW - u.s(3), rect.y + u.s(2), sbW, rect.h - u.s(4) };
            u.dl->AddRectFilled(track, NkScrollTrack(), sbW * 0.5f);
            float32 thh = viewH * (viewH / contentH); if (thh < u.s(24)) thh = u.s(24);
            float32 ty = track.y + (track.h - thh) * (w->codeSY / maxSY);
            const NkRect thumb = { track.x + u.s(2), ty, sbW - u.s(4), thh };
            if (!blockBg && u.click && u.Hit(thumb)) { w->codeBar = 1; w->codeBarOff = u.mp.y - ty; }
            if (w->codeBar == 1) { w->codeSY = ((u.mp.y - w->codeBarOff - track.y) / (track.h - thh)) * maxSY;
                if (w->codeSY < 0.f) w->codeSY = 0.f; if (w->codeSY > maxSY) w->codeSY = maxSY; ty = track.y + (track.h - thh) * (w->codeSY / maxSY); }
            u.dl->AddRectFilled({ track.x + u.s(2), ty, sbW - u.s(4), thh }, w->codeBar == 1 ? NkCol::primary : NkScrollThumb(false), (sbW - u.s(4)) * 0.5f);
        }
        if (maxSX > 0.5f) {
            const NkRect track = { rect.x + gutter, rect.y + rect.h - sbW - u.s(2), viewW, sbW };
            u.dl->AddRectFilled(track, NkScrollTrack(), sbW * 0.5f);
            float32 tww = viewW * (viewW / maxW); if (tww < u.s(24)) tww = u.s(24);
            float32 tx = track.x + (track.w - tww) * (w->codeSX / maxSX);
            const NkRect thumb = { tx, track.y + u.s(2), tww, sbW - u.s(4) };
            if (!blockBg && u.click && u.Hit(thumb)) { w->codeBar = 2; w->codeBarOff = u.mp.x - tx; }
            if (w->codeBar == 2) { w->codeSX = ((u.mp.x - w->codeBarOff - track.x) / (track.w - tww)) * maxSX;
                if (w->codeSX < 0.f) w->codeSX = 0.f; if (w->codeSX > maxSX) w->codeSX = maxSX; tx = track.x + (track.w - tww) * (w->codeSX / maxSX); }
            u.dl->AddRectFilled({ tx, track.y + u.s(2), tww, sbW - u.s(4) }, w->codeBar == 2 ? NkCol::primary : NkScrollThumb(false), (sbW - u.s(4)) * 0.5f);
        }
    }

    // ── Saisie clavier multi-ligne au caret. Modifie buf+caret. Renvoie true si le caret/contenu a bouge. ──
    inline bool NkWizCodeEditInput(const NkUi& u, NkString& buf, int32& caret, bool blockBg) {
        if (blockBg) return false;
        if (caret < 0) caret = 0; if (caret > (int32)buf.Size()) caret = (int32)buf.Size();
        const int32 caret0 = caret; const usize len0 = buf.Size();
        auto eraseAt  = [&](int32 i) { if (i < 0 || i >= (int32)buf.Size()) return; NkString o; const char* s = buf.CStr(); for (int32 k = 0; s[k]; ++k) if (k != i) o += s[k]; buf = o; };
        auto insertAt = [&](int32 i, char c) { NkString o; const char* s = buf.CStr(); for (int32 k = 0; k < i && s[k]; ++k) o += s[k]; o += c; for (int32 k = i; s[k]; ++k) o += s[k]; buf = o; };
        const int32 len = (int32)buf.Size(); const char* s0 = buf.CStr();
        if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Left)  && caret > 0)   --caret;
        if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Right) && caret < len) ++caret;
        if (u.ctx->input.KeyPressed(NkGuiKey::Home)) { while (caret > 0 && s0[caret - 1] != '\n') --caret; }
        if (u.ctx->input.KeyPressed(NkGuiKey::End))  { while (caret < len && s0[caret] != '\n') ++caret; }
        if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Up) || u.ctx->input.KeyPressedRepeat(NkGuiKey::Down)) {
            int32 col = 0; { int32 k = caret; while (k > 0 && s0[k - 1] != '\n') { --k; ++col; } }
            if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Up)) {
                const int32 ls = caret - col; if (ls > 0) { int32 ps = ls - 1; while (ps > 0 && s0[ps - 1] != '\n') --ps; int32 pl = 0; while (s0[ps + pl] && s0[ps + pl] != '\n') ++pl; caret = ps + (col < pl ? col : pl); }
            } else { int32 le = caret; while (le < len && s0[le] != '\n') ++le; if (le < len) { const int32 ns = le + 1; int32 nl = 0; while (s0[ns + nl] && s0[ns + nl] != '\n') ++nl; caret = ns + (col < nl ? col : nl); } }
        }
        if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Backspace) && caret > 0) { eraseAt(caret - 1); --caret; }
        if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Delete) && caret < (int32)buf.Size()) eraseAt(caret);
        if (u.ctx->input.KeyPressed(NkGuiKey::Enter)) { insertAt(caret, '\n'); ++caret; }
        if (u.ctx->input.KeyPressed(NkGuiKey::Tab))   { for (int32 t = 0; t < 4; ++t) { insertAt(caret, ' '); ++caret; } }
        if (u.ctx->input.wantPaste) { const NkString c = u.ctx->GetClipboard(); const char* s = c.CStr(); for (int32 k = 0; s[k]; ++k) { if (s[k] == '\r') continue; insertAt(caret, s[k]); ++caret; } u.ctx->input.wantPaste = false; }
        for (int32 i = 0; i < u.ctx->input.charCount; ++i) { const uint32 cp = u.ctx->input.chars[i]; if (cp >= 32 && cp < 127) { insertAt(caret, (char)cp); ++caret; } }
        return caret != caret0 || buf.Size() != len0;
    }

    // ── Molette + scrollbars V/H d'une COLONNE autonome (appelee APRES le rendu, clip deja ferme). ──
    inline void NkWizColScroll(const NkUi& u, const NkRect& rect, float32 contentH, float32 contentW, float32& sy, float32& sx, bool blockBg) {
        const float32 sb = u.s(10);
        const float32 viewH = rect.h, viewW = rect.w - sb - u.s(4);
        const float32 maxSY = (contentH > viewH) ? contentH - viewH : 0.f;
        const float32 maxSX = (contentW > viewW) ? contentW - viewW : 0.f;
        if (!blockBg && u.Hit(rect) && u.ctx->input.wheel != 0.f) {
            if (u.ctx->input.shiftDown) sx -= u.ctx->input.wheel * u.s(40);
            else                        sy -= u.ctx->input.wheel * u.s(40);
            u.ctx->input.wheel = 0.f;
        }
        if (sy < 0.f) sy = 0.f; if (sy > maxSY) sy = maxSY;
        if (sx < 0.f) sx = 0.f; if (sx > maxSX) sx = maxSX;
        if (maxSY > 0.5f) {
            const NkRect track = { rect.x + rect.w - sb - u.s(2), rect.y + u.s(1), sb, rect.h - u.s(2) };
            u.dl->AddRectFilled(track, NkScrollTrack(), sb * 0.5f);
            float32 thh = viewH * (viewH / contentH); if (thh < u.s(26)) thh = u.s(26);
            const float32 ty = track.y + (track.h - thh) * (sy / maxSY);
            const bool thHov = u.Hit({ track.x + u.s(2), ty, sb - u.s(4), thh });
            u.dl->AddRectFilled({ track.x + u.s(2), ty, sb - u.s(4), thh }, NkScrollThumb(thHov), (sb - u.s(4)) * 0.5f);
        }
        if (maxSX > 0.5f) {
            const NkRect track = { rect.x + u.s(1), rect.y + rect.h - sb - u.s(1), viewW, sb };
            u.dl->AddRectFilled(track, NkColor{ 18,21,26,150 }, sb * 0.5f);
            float32 tww = viewW * (viewW / contentW); if (tww < u.s(26)) tww = u.s(26);
            const float32 tx = track.x + (track.w - tww) * (sx / maxSX);
            u.dl->AddRectFilled({ tx, track.y + u.s(2), tww, sb - u.s(4) }, NkColor{ 70,76,84,255 }, (sb - u.s(4)) * 0.5f);
        }
    }

    // ── PROJET — Etape 1 : Definition. DEUX colonnes a scroll V/H independant. ──
    inline float32 NkProjDef(const NkUi& u, const NkRect& view, NkNewWsState* w, NkCodeDialogs* dlg, float32 dt, bool blockBg, const NkIcons& ic) {
        (void)dlg;
        NkWizProject& p = w->projDraft;
        const float32 rightW = (view.w > u.s(760)) ? u.s(300) : u.s(250);
        const float32 fH = u.s(30), sb = u.s(10);
        const float32 rightFullW = rightW + sb + u.s(12);
        const NkRect leftRect  = { view.x + u.s(12), view.y + u.s(6), view.w - rightFullW - u.s(22), view.h - u.s(12) };
        const NkRect rightRect = { view.x + view.w - rightFullW - u.s(6), view.y + u.s(6), rightFullW, view.h - u.s(12) };

        // ===== COLONNE GAUCHE =====
        const float32 cx = leftRect.x + u.s(8) - w->fltLSX;
        const float32 cw = leftRect.w - u.s(16) - sb;
        const float32 leftTop = leftRect.y + u.s(8);
        u.dl->PushClipRect(leftRect, true);
        float32 y = leftTop - w->fltLSY;
        NkWizLabel(u, cx, y, NkT("nws.projname")); y += u.s(20);
        NkWizField(u, { cx, y, cw, fH }, p.name, (int32)sizeof(p.name), 100, w, dt, blockBg, u.s(10)); y += fH + u.s(16);
        NkWizLabel(u, cx, y, NkT("nws.projkind")); y += u.s(22);
        { int32 nK = 0; const char* const* kinds = NkWizKinds(&nK);
          const int32 nShown = (nK > 4 ? 4 : nK);   // 'test' retire de la liste : c'est un toggle on/off
          if (p.kind >= nShown) p.kind = 0;
          const float32 gap = u.s(8), kcw = (cw - gap * (nShown - 1)) / nShown, kch = u.s(52);
          for (int32 k = 0; k < nShown; ++k) {
              const NkRect c = { cx + k * (kcw + gap), y, kcw, kch };
              const bool selk = (p.kind == k); const bool hv = !blockBg && u.Hit(c);
              u.Panel(c, selk ? NkCol::secondary : NkCol::input, selk ? NkCol::primary : NkCol::border, NkR::md * u.S);
              NkOwIco(u, NkWizKindTex(ic, k), NkWizKindIcon(k), { c.x + (kcw - u.s(20)) * 0.5f, c.y + u.s(8), u.s(20), u.s(20) }, selk ? NkCol::primary : NkCol::mutedFg);
              const float32 tw = u.TextW(kinds[k]); u.Text(c.x + (kcw - tw) * 0.5f, c.y + u.s(32), kinds[k], selk ? NkCol::primary : NkCol::mutedFg);
              if (hv && u.click) p.kind = k;
          }
          y += kch + u.s(8);
          const char* desc[] = { NkT("nws.kind0"), NkT("nws.kind1"), NkT("nws.kind2"), NkT("nws.kind3"), NkT("nws.kind4") };
          NkWizHint(u, cx, y, cw, (p.kind >= 0 && p.kind < 5) ? desc[p.kind] : ""); y += u.s(28); }
        NkWizLabel(u, cx, y, NkT("nws.langdialect")); y += u.s(20);
        { int32 nL = 0, nD = 0; const char* const* langs = NkWizLangs(&nL); const char* const* dials = NkWizDialects(&nD);
          const float32 cwid = (cw - u.s(16)) * 0.5f;
          u.Text(cx, y, NkT("gen.language"), NkCol::mutedFg); u.Text(cx + cwid + u.s(16), y, NkT("nws.dialect"), NkCol::mutedFg);
          NkWizCombo(u, { cx, y + u.s(16), cwid, fH }, 110, w, langs, nL, &p.lang, blockBg);
          NkWizCombo(u, { cx + cwid + u.s(16), y + u.s(16), cwid, fH }, 111, w, dials, nD, &p.dialect, blockBg);
          y += u.s(16) + fH + u.s(16); }
        NkWizLabel(u, cx, y, NkT("nws.projloc")); y += u.s(20);
        NkWizField(u, { cx, y, cw - u.s(38), fH }, p.location, (int32)sizeof(p.location), 101, w, dt, blockBg, u.s(10));
        { const NkRect br = { cx + cw - u.s(32), y, u.s(32), fH }; u.Rect(br, NkCol::muted, NkR::md * u.S);
          NkOwIco(u, ic.ouvrirDossier, "folder", { br.x + u.s(9), br.y + u.s(9), u.s(14), u.s(14) }, NkCol::mutedFg); }
        y += fH + u.s(16);
        NkWizLabel(u, cx, y, NkT("nws.sources")); y += u.s(20);
        y += NkWizEditList(u, cx, y, cw, p.sources, w->projSrcAdd, (int32)sizeof(w->projSrcAdd), 102, w, dt, blockBg, ic.plus) + u.s(14);
        NkWizLabel(u, cx, y, NkT("nws.includes")); y += u.s(20);
        y += NkWizEditList(u, cx, y, cw, p.includes, w->projIncAdd, (int32)sizeof(w->projIncAdd), 103, w, dt, blockBg, ic.plus) + u.s(8);

        // Fin colonne gauche : clip + molette + scrollbars.
        const float32 leftContentH = (y + w->fltLSY) - leftTop + u.s(20);
        u.dl->PopClipRect();
        NkWizColScroll(u, leftRect, leftContentH, cw + u.s(20), w->fltLSY, w->fltLSX, blockBg);

        // ===== COLONNE DROITE =====
        const float32 rx = rightRect.x + u.s(4) - w->fltRSX;
        const float32 rightTop = rightRect.y + u.s(8);
        u.dl->PushClipRect(rightRect, true);
        float32 ry = rightTop - w->fltRSY;
        NkWizLabel(u, rx, ry, NkT("nws.builddirs")); ry += u.s(20);
        { const float32 fh2 = u.s(28);
          u.Text(rx, ry, NkT("nws.objdir"), NkCol::mutedFg); ry += u.s(16);
          NkWizField(u, { rx, ry, rightW, fh2 }, p.objDir, (int32)sizeof(p.objDir), 130, w, dt, blockBg, u.s(10)); ry += fh2 + u.s(8);
          u.Text(rx, ry, NkT("nws.targetdir"), NkCol::mutedFg); ry += u.s(16);
          NkWizField(u, { rx, ry, rightW, fh2 }, p.tgtDir, (int32)sizeof(p.tgtDir), 131, w, dt, blockBg, u.s(10)); ry += fh2 + u.s(8);
          // Bin : par defaut identique a target ; lien pour resynchroniser.
          u.Text(rx, ry, NkT("nws.bindir"), NkCol::mutedFg);
          { const char* lk = StrEq(p.binDir, p.tgtDir) ? "= target" : "resync = target"; const float32 lw = u.TextW(lk);
            const NkRect lr = { rx + rightW - lw - u.s(8), ry - u.s(3), lw + u.s(8), u.s(16) };
            const bool lhv = !blockBg && u.Hit(lr);
            u.TextV(lr.x + u.s(4), lr.y, u.s(16), lk, lhv ? NkCol::primary : NkCol::mutedFg);
            if (lhv && u.click) { std::snprintf(p.binDir, sizeof(p.binDir), "%s", p.tgtDir); w->focus = -1; } }
          ry += u.s(16);
          NkWizField(u, { rx, ry, rightW, fh2 }, p.binDir, (int32)sizeof(p.binDir), 132, w, dt, blockBg, u.s(10)); ry += fh2 + u.s(10); }
        NkWizLabel(u, rx, ry, NkT("nws.deps")); ry += u.s(20);
        // On ne peut dependre que d'une bibliotheque (staticlib/sharedlib), jamais d'une application.
        { int32 nLibs = 0; for (usize i = 0; i < w->projects.Size(); ++i) { if ((int32)i == w->projEditIdx) continue; const int32 k = w->projects[i].kind; if (k == 2 || k == 3) ++nLibs; }
          const float32 boxH = u.s(40) + (nLibs == 0 ? u.s(22) : nLibs * u.s(26));
          const NkRect box = { rx, ry, rightW, boxH };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          float32 dy = box.y + u.s(14);
          for (usize i = 0; i < w->projects.Size(); ++i) {
              if ((int32)i == w->projEditIdx) continue;
              const int32 k = w->projects[i].kind; if (k != 2 && k != 3) continue;   // seules les libs
              const char* nm = w->projects[i].name; bool dep = false; int32 di = -1;
              for (usize j = 0; j < p.dependsOn.Size(); ++j) if (StrEq(p.dependsOn[j].CStr(), nm)) { dep = true; di = (int32)j; break; }
              if (NkWizCheck(u, box.x + u.s(12), dy, nm, dep, blockBg)) { if (dep && di >= 0) p.dependsOn.Erase(p.dependsOn.Begin() + di); else p.dependsOn.PushBack(NkString(nm)); }
              // icone du type de projet (a droite de la ligne)
              NkOwIco(u, NkWizKindTex(ic, k), NkWizKindIcon(k), { box.x + rightW - u.s(26), dy - u.s(7), u.s(14), u.s(14) }, dep ? NkCol::accent : NkCol::mutedFg);
              dy += u.s(26);
          }
          if (nLibs == 0) { u.TextV(box.x + u.s(12), dy - u.s(8), u.s(22), "(aucune bibliotheque disponible)", NkCol::mutedFg); }
          ry = box.y + boxH + u.s(16); }
        NkWizLabel(u, rx, ry, NkT("nws.genfiles")); ry += u.s(20);
        { const bool hasMain = (p.kind == 0 || p.kind == 1);   // seuls console/windowed ont un main()
          const bool isTestKind = (p.kind == 4);               // un projet "test" EST deja une suite de tests
          if (!hasMain) p.genMain = false;
          const int32 nrows = (hasMain ? 1 : 0) + 1 + (isTestKind ? 0 : 1);
          const float32 boxH = u.s(14) + nrows * u.s(26) + (isTestKind ? 0.f : u.s(8));
          const NkRect box = { rx, ry, rightW, boxH };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          float32 gy = box.y + u.s(18);
          if (hasMain) { if (NkWizCheck(u, box.x + u.s(12), gy, NkT("nws.genmain"), p.genMain, blockBg)) p.genMain = !p.genMain; gy += u.s(26); }
          if (NkWizCheck(u, box.x + u.s(12), gy, NkT("nws.genreadme"), p.genReadme, blockBg)) p.genReadme = !p.genReadme; gy += u.s(26);
          // Tests Unitest : toggle (bouton + icone), disponible pour console/windowed/static/shared.
          if (!isTestKind) {
              const NkRect tg = { box.x + u.s(12), gy - u.s(2), rightW - u.s(24), u.s(28) };
              const bool thv = !blockBg && u.Hit(tg);
              u.Panel(tg, p.genTest ? NkCol::secondary : NkCol::input, p.genTest ? NkCol::primary : NkCol::border, NkR::md * u.S);
              NkOwIco(u, ic.kTest, "check-circle", { tg.x + u.s(8), tg.y + (tg.h - u.s(14)) * 0.5f, u.s(14), u.s(14) }, p.genTest ? NkCol::primary : NkCol::mutedFg);
              u.TextV(tg.x + u.s(28), tg.y, tg.h, NkT("nws.tests"), p.genTest ? NkCol::primary : NkCol::foreground);
              const char* st = p.genTest ? "ON" : "OFF";
              u.TextV(tg.x + tg.w - u.TextW(st) - u.s(10), tg.y, tg.h, st, p.genTest ? NkCol::success : NkCol::mutedFg);
              if (thv && u.click) p.genTest = !p.genTest;
          }
          ry += boxH + u.s(10); }
        // Proprietes des tests (visibles seulement si « Tests Unitest » est active) — facon Jenga.
        if (p.genTest) {
            NkWizLabel(u, rx, ry, NkT("nws.testprops")); ry += u.s(20);
            const NkRect box = { rx, ry, rightW, u.s(98) };
            u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
            u.Text(box.x + u.s(12), box.y + u.s(10), NkT("nws.testfiles"), NkCol::mutedFg);
            NkWizField(u, { box.x + u.s(12), box.y + u.s(28), rightW - u.s(24), u.s(28) }, p.testFiles, (int32)sizeof(p.testFiles), 140, w, dt, blockBg, u.s(10));
            if (NkWizCheck(u, box.x + u.s(12), box.y + u.s(76), NkT("nws.desktoponly"), p.testDesktopOnly, blockBg)) p.testDesktopOnly = !p.testDesktopOnly;
            ry += u.s(108);
        }

        // Fin colonne droite : clip + molette + scrollbars.
        const float32 rightContentH = (ry + w->fltRSY) - rightTop + u.s(20);
        u.dl->PopClipRect();
        NkWizColScroll(u, rightRect, rightContentH, rightW + u.s(16), w->fltRSY, w->fltRSX, blockBg);
        return view.h;   // colonnes auto-gerees -> pas de scroll modal global
    }

    // ── PROJET — Etape 2 : Filtres par plateforme + par config. DEUX colonnes a scroll V/H independant. ──
    inline float32 NkProjFilters(const NkUi& u, const NkRect& view, NkNewWsState* w, float32 dt, bool blockBg, const NkIcons& ic) {
        NkWizProject& p = w->projDraft;
        const float32 rightW = (view.w > u.s(760)) ? u.s(300) : u.s(250);
        const float32 fH = u.s(28), sb = u.s(10), colGap = u.s(16);
        int32 nTc = 0; const char* const* tcs = NkWizTcs(&nTc);
        int32 nK = 0; const char* const* kinds = NkWizKinds(&nK);

        const float32 rightFullW = rightW + sb + u.s(12);
        const NkRect leftRect  = { view.x + u.s(12), view.y + u.s(6), view.w - rightFullW - u.s(22), view.h - u.s(12) };
        const NkRect rightRect = { view.x + view.w - rightFullW - u.s(6), view.y + u.s(6), rightFullW, view.h - u.s(12) };

        // ===== COLONNE GAUCHE (plateformes + filtres personnalises) =====
        const float32 cx = leftRect.x + u.s(8) - w->fltLSX;
        const float32 cw = leftRect.w - u.s(16) - sb;
        const float32 leftTop = leftRect.y + u.s(8);
        float32 leftMaxX = 0.f; (void)leftMaxX;
        u.dl->PushClipRect(leftRect, true);
        float32 y = leftTop - w->fltLSY;
        NkWizLabel(u, cx, y, NkT("nws.platfilters")); u.Text(cx + u.TextW(NkT("nws.platfilters")) + u.s(8), y, NkT("nws.filterhint"), NkCol::mutedFg); y += u.s(24);
        const int32 nOs = NkWizOsCount();
        int32 shownOs = 0;
        for (int32 oi = 0; oi < nOs; ++oi) {
            NkWizProject::OsFlt& o = p.os[oi];
            if (!o.on) continue;
            ++shownOs;
            const int32 osSp = NkWizOsSpecial(oi);
            const bool isAndroid = (osSp == 1), isWeb = (osSp == 2);
            // Hauteur ADAPTATIVE : les pills (links/defines) qui s'enroulent restent visibles, bouton + compris.
            const float32 pillsW = cw - u.s(140);
            const float32 linksH = NkProjPillsH(u, pillsW, o.links,   w->projFltTarget == oi * 2);
            const float32 defsH  = NkProjPillsH(u, pillsW, o.defines, w->projFltTarget == oi * 2 + 1);
            float32 hh = u.s(34) + (isAndroid ? u.s(34) : 0.f) + u.s(34) + linksH + u.s(6) + defsH + u.s(10);
            const float32 inner = hh;
            const NkRect box = { cx, y, cw, inner };
            u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
            u.Text(box.x + u.s(14), box.y + u.s(10), NkWizOsName(oi), NkCol::foreground);
            u.Text(box.x + u.s(14) + u.TextW(NkWizOsName(oi)) + u.s(8), box.y + u.s(10), NkWizOsFilt(oi), NkCol::mutedFg);
            // Bouton supprimer ce filtre systeme.
            { const NkRect xr = { box.x + cw - u.s(24), box.y + u.s(8), u.s(16), u.s(16) }; const bool xhv = !blockBg && u.Hit(xr);
              u.Icon("x", { xr.x + u.s(3), xr.y + u.s(3), u.s(10), u.s(10) }, xhv ? NkCol::danger : NkCol::mutedFg);
              if (xhv && u.click) { o.on = false; } }
            float32 fy = box.y + u.s(34);
            if (isAndroid) { u.Text(box.x + u.s(14), fy + u.s(6), "Type override", NkCol::mutedFg);
                NkWizCombo(u, { box.x + u.s(120), fy, u.s(150), fH }, 600 + oi * 10 + 1, w, kinds, nK, &o.androidKind, blockBg); fy += u.s(34); }
            u.Text(box.x + u.s(14), fy + u.s(6), "Toolchain", NkCol::mutedFg);
            NkWizCombo(u, { box.x + u.s(120), fy, u.s(160), fH }, 600 + oi * 10, w, tcs, nTc, &o.toolchain, blockBg);
            // Toolchain liee a un AUTRE OS -> avertissement (traitement special).
            { const int32 tcOs = NkWizTcOs(o.toolchain); if (tcOs >= 0 && tcOs != oi) {
                u.Icon("alert-triangle", { box.x + u.s(286), fy + u.s(6), u.s(13), u.s(13) }, NkCol::accent);
                u.TextEllipsis(box.x + u.s(304), fy + u.s(6), cw - u.s(320), NkT("nws.tcotheros"), NkCol::accent); } }
            if (isWeb) { u.Text(box.x + u.s(286), fy + u.s(6), "Mem", NkCol::mutedFg);
                int32 nM = 0; const char* const* mem = NkWizMem(&nM);
                NkWizCombo(u, { box.x + u.s(322), fy, u.s(90), fH }, 600 + oi * 10 + 2, w, mem, nM, &o.webMem, blockBg); }
            fy += u.s(34);
            u.Text(box.x + u.s(14), fy + u.s(2), "Links", NkCol::mutedFg);
            NkProjPills(u, box.x + u.s(120), fy, pillsW, o.links, oi * 2, w, dt, blockBg, ic.plus); fy += linksH + u.s(6);
            u.Text(box.x + u.s(14), fy + u.s(2), "Defines", NkCol::mutedFg);
            NkProjPills(u, box.x + u.s(120), fy, pillsW, o.defines, oi * 2 + 1, w, dt, blockBg, ic.plus); fy += defsH;
            y += inner + u.s(10);
        }
        if (!shownOs) { u.TextV(cx, y, u.s(22), "(aucun filtre systeme — ajoutez-en ci-dessous)", NkCol::mutedFg); y += u.s(28); }
        // Ajouter un filtre systeme : plateformes encore absentes (chips cliquables).
        { bool anyOff = false; for (int32 oi = 0; oi < nOs; ++oi) if (!p.os[oi].on) { anyOff = true; break; }
          if (anyOff) {
              NkWizLabel(u, cx, y, NkT("nws.addsysfilter")); y += u.s(22);
              float32 chx = cx, chy = y; const float32 chh = u.s(26);
              for (int32 oi = 0; oi < nOs; ++oi) { if (p.os[oi].on) continue;
                  const char* nm = NkWizOsName(oi); const float32 cw2 = u.TextW(nm) + u.s(30);
                  if (chx + cw2 > cx + cw) { chx = cx; chy += chh + u.s(6); }
                  const NkRect b = { chx, chy, cw2, chh }; const bool hv = !blockBg && u.Hit(b);
                  u.Panel(b, hv ? NkCol::hover : NkCol::input, NkCol::border, NkR::md * u.S);
                  NkOwIco(u, ic.plus, "plus", { b.x + u.s(8), b.y + (chh - u.s(11)) * 0.5f, u.s(11), u.s(11) }, hv ? NkCol::foreground : NkCol::primary);
                  u.TextV(b.x + u.s(22), b.y, chh, nm, hv ? NkCol::foreground : NkCol::mutedFg);
                  if (hv && u.click) p.os[oi].on = true;
                  chx += cw2 + u.s(6);
              }
              y = chy + chh + u.s(16);
          } }
        // Filtres personnalises : EXPRESSION + toolchain + CONTENU (defines / links) + Valider/Supprimer.
        for (usize c = 0; c < p.custFlt.Size(); ++c) {
            NkWizProject::CustFlt& cf = p.custFlt[c];
            const NkRect box = { cx, y, cw, u.s(214) };
            u.Panel(box, NkCol::surface, cf.ok ? NkCol::success : NkCol::accent, NkR::md * u.S);
            u.Text(box.x + u.s(14), box.y + u.s(10), NkT("nws.customfilter"), cf.ok ? NkCol::success : NkCol::accent);
            if (cf.ok) NkOwIco(u, ic.valideSimple, "check", { box.x + u.s(14) + u.TextW(NkT("nws.customfilter")) + u.s(8), box.y + u.s(9), u.s(14), u.s(14) }, NkCol::success);
            u.Text(box.x + u.s(14), box.y + u.s(32), NkT("nws.expression"), NkCol::mutedFg);
            NkWizField(u, { box.x + u.s(14), box.y + u.s(46), cw - u.s(28), u.s(26) }, cf.expr, (int32)sizeof(cf.expr), 400 + (int32)c, w, dt, blockBg, u.s(10));
            // Toolchain (OS-dependante = traitement special).
            u.Text(box.x + u.s(14), box.y + u.s(82), "Toolchain", NkCol::mutedFg);
            NkWizCombo(u, { box.x + u.s(90), box.y + u.s(78), u.s(160), fH }, 700 + (int32)c, w, tcs, nTc, &cf.toolchain, blockBg);
            { const int32 tcOs = NkWizTcOs(cf.toolchain);
              if (tcOs >= 0 && !NkString(cf.expr).Contains(NkWizOsFilt(tcOs))) {
                u.Icon("alert-triangle", { box.x + u.s(262), box.y + u.s(84), u.s(13), u.s(13) }, NkCol::accent);
                char ht[80]; std::snprintf(ht, sizeof(ht), "ajoutez %s a l'expression", NkWizOsFilt(tcOs));
                u.TextEllipsis(box.x + u.s(280), box.y + u.s(84), cw - u.s(300), ht, NkCol::accent); } }
            u.Text(box.x + u.s(14), box.y + u.s(116), "Defines", NkCol::mutedFg);
            NkProjPills(u, box.x + u.s(90), box.y + u.s(114), cw - u.s(110), cf.defines, 500 + (int32)c * 2, w, dt, blockBg, ic.plus);
            u.Text(box.x + u.s(14), box.y + u.s(148), "Links", NkCol::mutedFg);
            NkProjPills(u, box.x + u.s(90), box.y + u.s(146), cw - u.s(110), cf.links, 500 + (int32)c * 2 + 1, w, dt, blockBg, ic.plus);
            // Boutons Valider / Supprimer.
            const float32 by = box.y + u.s(178), bh = u.s(28);
            const NkRect vr = { box.x + u.s(14), by, u.s(120), bh };
            const bool vhv = !blockBg && u.Hit(vr);
            u.Rect(vr, vhv ? NkColHover(NkCol::success) : NkCol::success, NkR::md * u.S);
            NkOwIco(u, ic.valideSimple, "check", { vr.x + u.s(14), vr.y + (bh - u.s(14)) * 0.5f, u.s(14), u.s(14) }, NkCol::primaryFg);
            u.TextV(vr.x + u.s(34), vr.y, bh, NkT("btn.validate"), NkCol::primaryFg);
            if (vhv && u.click) { cf.ok = (cf.expr[0] != 0); w->focus = -1; }
            const NkRect dr = { box.x + u.s(144), by, u.s(120), bh };
            const bool dhv = !blockBg && u.Hit(dr);
            u.Rect(dr, dhv ? NkColor{ 200,60,60,255 } : NkCol::muted, NkR::md * u.S);
            u.Icon("trash", { dr.x + u.s(14), dr.y + (bh - u.s(14)) * 0.5f, u.s(14), u.s(14) }, dhv ? NkCol::primaryFg : NkCol::danger);
            u.TextV(dr.x + u.s(34), dr.y, bh, NkT("tc.delete"), dhv ? NkCol::primaryFg : NkCol::foreground);
            if (dhv && u.click) { p.custFlt.Erase(p.custFlt.Begin() + c); break; }
            y += u.s(224);
        }
        { const NkRect ar = { cx, y, u.s(240), u.s(28) }; const bool hv = !blockBg && u.Hit(ar);
          NkOwIco(u, ic.plus, "plus", { ar.x, ar.y + u.s(8), u.s(12), u.s(12) }, hv ? NkCol::foreground : NkCol::primary);
          u.TextV(ar.x + u.s(18), ar.y, u.s(28), NkT("nws.addcustomfilter"), hv ? NkCol::foreground : NkCol::primary);
          if (hv && u.click) { NkWizProject::CustFlt cf; p.custFlt.PushBack(cf); } y += u.s(34); }

        // Fin colonne gauche : fermer le clip, gerer molette + scrollbars (apres rendu).
        const float32 leftContentH = (y + w->fltLSY) - leftTop + u.s(20);
        u.dl->PopClipRect();
        NkWizColScroll(u, leftRect, leftContentH, cw + u.s(20), w->fltLSY, w->fltLSX, blockBg);

        // ===== COLONNE DROITE : zone « filtres de config » (scroll V/H PROPRE) + apercu code =====
        const float32 rLabelX = rightRect.x + u.s(4);
        int32 nO = 0; const char* const* opt = NkWizOpt(&nO);
        NkWizLabel(u, rLabelX, rightRect.y, NkT("nws.cfgfilters"));
        const float32 czY = rightRect.y + u.s(22);
        const float32 czH = (rightRect.h - u.s(22)) * 0.52f;
        const NkRect cfgZone = { rightRect.x, czY, rightRect.w, czH };
        u.Panel(cfgZone, NkColor{ 14,17,22,170 }, NkCol::border, NkR::md * u.S);
        const float32 sbz = u.s(10);
        const float32 cfgVisW = cfgZone.w - sbz - u.s(8);
        // Les pills s'enroulent dans la largeur VISIBLE -> le bouton + et les defines restent accessibles
        // (scroll V). On n'elargit la boite (scroll H) que si UNE pill seule depasse la zone visible.
        const float32 cfgPillsW = cfgVisW - u.s(92);
        float32 cfgBoxW = cfgVisW;
        for (usize c = 0; c < p.cfgFlt.Size(); ++c) { const NkWizProject::CfgFlt& cf = p.cfgFlt[c];
            for (usize d = 0; d < cf.defines.Size(); ++d) { const float32 wv = u.s(92) + u.TextW(cf.defines[d].CStr()) + u.s(50); if (wv > cfgBoxW) cfgBoxW = wv; } }
        u.dl->PushClipRect(cfgZone, true);
        const float32 crx = cfgZone.x + u.s(8) - w->cfgSX;
        const float32 cfgTop = cfgZone.y + u.s(8);
        float32 ry = cfgTop - w->cfgSY;
        for (usize c = 0; c < p.cfgFlt.Size(); ++c) {
            NkWizProject::CfgFlt& cf = p.cfgFlt[c];
            const float32 defH = NkProjPillsH(u, cfgPillsW, cf.defines, w->projFltTarget == 320 + (int32)c * 2);
            const float32 boxH = u.s(34) + defH + u.s(6) + u.s(34) + u.s(34) + u.s(8);
            const NkRect box = { crx, ry, cfgBoxW, boxH };
            u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
            u.Text(box.x + u.s(12), box.y + u.s(10), cf.name.CStr(), NkCol::foreground);
            char ff[80]; std::snprintf(ff, sizeof(ff), "config:%s", cf.name.CStr());
            u.Text(box.x + u.s(12) + u.TextW(cf.name.CStr()) + u.s(8), box.y + u.s(10), ff, NkCol::mutedFg);
            float32 cyy = box.y + u.s(34);
            u.Text(box.x + u.s(12), cyy, "Defines", NkCol::mutedFg);
            NkProjPills(u, box.x + u.s(80), cyy - u.s(2), cfgPillsW, cf.defines, 320 + (int32)c * 2, w, dt, blockBg, ic.plus);
            cyy += defH + u.s(6);
            u.Text(box.x + u.s(12), cyy + u.s(4), "Optimize", NkCol::mutedFg);
            NkWizCombo(u, { box.x + u.s(80), cyy, u.s(110), fH }, 360 + (int32)c, w, opt, nO, &cf.opt, blockBg); cyy += u.s(34);
            u.Text(box.x + u.s(12), cyy + u.s(4), "Symbols", NkCol::mutedFg);
            if (NkWizCheck(u, box.x + u.s(80), cyy + u.s(10), "Activer", cf.sym, blockBg)) cf.sym = !cf.sym;
            ry += boxH + u.s(10);
        }
        if (p.cfgFlt.Empty()) { u.TextV(crx, ry, u.s(22), "(aucune configuration cochee a l'etape 1)", NkCol::mutedFg); ry += u.s(26); }
        const float32 cfgContentH = (ry + w->cfgSY) - cfgTop + u.s(12);
        u.dl->PopClipRect();
        NkWizColScroll(u, cfgZone, cfgContentH, cfgBoxW + u.s(16), w->cfgSY, w->cfgSX, blockBg);

        // Apercu COMPLET du code genere (projet + TOUS les filtres) : scroll V/H + coloration.
        const float32 apY = cfgZone.y + cfgZone.h + u.s(10);
        NkWizLabel(u, rLabelX, apY, NkT("nws.codepreview"));
        { const NkString code = w->BuildProjectJenga(p);
          const float32 apBoxY = apY + u.s(20);
          float32 apH = (rightRect.y + rightRect.h) - apBoxY - u.s(2); if (apH < u.s(120)) apH = u.s(120);
          NkWizCodeBox(u, { rightRect.x, apBoxY, rightRect.w - u.s(2), apH }, code, w, blockBg, true); }
        return view.h;   // colonnes auto-gerees -> pas de scroll modal global pour cette etape
    }

    // ── PROJET — Etape 3 : Resume (apercu .jenga + ce qui sera cree). ──
    inline float32 NkProjSummary(const NkUi& u, const NkRect& body, NkNewWsState* w, float32 dt, bool blockBg, const NkIcons& ic) {
        NkWizProject& p = w->projDraft;
        const float32 rightW = (body.w > u.s(760)) ? u.s(300) : u.s(250);
        const float32 cx = body.x + u.s(24), rx = body.x + body.w - u.s(24) - rightW, cw = (rx - u.s(28)) - cx;
        float32 y = body.y + u.s(14);
        // Choix : integre au workspace OU fichier .jenga separe (avec imports).
        NkWizLabel(u, cx, y, NkT("nws.insertformat")); y += u.s(22);
        { const float32 bw = (cw - u.s(10)) * 0.5f;
          struct Opt { const char* lab; bool sep; };
          const Opt opts[] = { { "Integre au workspace", false }, { "Fichier .jenga separe", true } };
          for (int32 i = 0; i < 2; ++i) {
              const NkRect b = { cx + i * (bw + u.s(10)), y, bw, u.s(34) };
              const bool selb = (p.separateFile == opts[i].sep); const bool hv = !blockBg && u.Hit(b);
              u.Panel(b, selb ? NkCol::secondary : (hv ? NkCol::hover : NkCol::input), selb ? NkCol::primary : NkCol::border, NkR::md * u.S);
              u.TextV(b.x + (bw - u.TextW(opts[i].lab)) * 0.5f, b.y, u.s(34), opts[i].lab, selb ? NkCol::primary : NkCol::foreground);
              // Changer de format -> revenir au code genere du nouveau format + remonter (imports visibles).
              if (hv && u.click && p.separateFile != opts[i].sep) { p.separateFile = opts[i].sep; w->codeSX = w->codeSY = 0.f; w->extractEdited = false; w->extractEditing = false; p.hasCustomCode = false; p.customCode.Clear(); }
          }
          y += u.s(44); }
        char hdr[120]; std::snprintf(hdr, sizeof(hdr), "- %s", p.separateFile ? "fichier separe (avec imports)" : "insere a la fin du workspace");
        NkWizLabel(u, cx, y, NkT("nws.jengaextract")); u.Text(cx + u.TextW(NkT("nws.jengaextract")) + u.s(8), y, hdr, NkCol::mutedFg);
        // Boutons : Modifier/Terminer + Regenerer (a droite du libelle).
        { const char* el = w->extractEditing ? "Terminer" : "Modifier"; const float32 ew = u.TextW(el) + u.s(30);
          const NkRect er = { cx + cw - ew, y - u.s(5), ew, u.s(22) }; const bool ehv = !blockBg && u.Hit(er);
          u.Rect(er, w->extractEditing ? NkCol::success : (ehv ? NkCol::hover : NkCol::muted), NkR::md * u.S);
          NkOwIco(u, w->extractEditing ? ic.valideSimple : ic.editer, w->extractEditing ? "check" : "edit", { er.x + u.s(8), er.y + u.s(4), u.s(13), u.s(13) }, w->extractEditing ? NkCol::primaryFg : NkCol::foreground);
          u.TextV(er.x + u.s(24), er.y, u.s(22), el, w->extractEditing ? NkCol::primaryFg : NkCol::foreground);
          if (ehv && u.click) { w->focusClaimed = true; if (!w->extractEditing) { w->extractBuf = p.separateFile ? w->BuildProjectFile(p) : w->BuildProjectJenga(p); w->extractCaret = 0; w->extractEditing = true; w->extractEdited = true; w->codeSX = w->codeSY = 0.f; } else w->extractEditing = false; }
          if (w->extractEdited) { const char* rl = "Regenerer"; const float32 rw = u.TextW(rl) + u.s(16);
              const NkRect rr = { cx + cw - ew - rw - u.s(8), y - u.s(5), rw, u.s(22) }; const bool rhv = !blockBg && u.Hit(rr);
              u.Rect(rr, rhv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
              u.TextV(rr.x + u.s(8), rr.y, u.s(22), rl, NkCol::mutedFg);
              if (rhv && u.click) { w->focusClaimed = true; w->extractEdited = false; w->extractEditing = false; } } }
        y += u.s(22);
        const float32 codeH = (body.h - (y - body.y) > u.s(220)) ? (body.h - (y - body.y) - u.s(20)) : u.s(220);
        if (w->extractEditing) {
            const bool moved = NkWizCodeEditInput(u, w->extractBuf, w->extractCaret, blockBg);
            w->blink += dt; if (w->blink > 1.f) w->blink -= 1.f;
            NkWizCodeBox(u, { cx, y, cw, codeH }, w->extractBuf, w, blockBg, true, &w->extractCaret, w->blink, moved);
        } else {
            const NkString code = w->extractEdited ? w->extractBuf : (p.separateFile ? w->BuildProjectFile(p) : w->BuildProjectJenga(p));
            NkWizCodeBox(u, { cx, y, cw, codeH }, code, w, blockBg, true);
        }
        y += codeH + u.s(10);

        // colonne droite : ce qui sera cree + resume
        float32 ry = body.y + u.s(14);
        NkWizLabel(u, rx, ry, NkT("nws.willcreate")); ry += u.s(22);
        auto created = [&](const char* path) { const NkRect b = { rx, ry, rightW, u.s(30) };
            u.Panel(b, NkCol::surface, NkCol::border, NkR::md * u.S);
            NkOwIco(u, ic.valideSimple, "check-circle", { b.x + u.s(10), b.y + u.s(8), u.s(14), u.s(14) }, NkCol::success);
            u.TextV(b.x + u.s(32), b.y, u.s(30), path, NkCol::foreground); ry += u.s(36); };
        char b1[256]; if (p.genMain) { std::snprintf(b1, sizeof(b1), "%ssrc/main.cpp", p.location); created(b1); }
        { char b2[256]; std::snprintf(b2, sizeof(b2), "%sinclude/", p.location); created(b2); }
        if (p.genReadme) { char b3[256]; std::snprintf(b3, sizeof(b3), "%sREADME.md", p.location); created(b3); }
        ry += u.s(10);
        NkWizLabel(u, rx, ry, NkT("nws.projsummary")); ry += u.s(22);
        auto chip = [&](uint32 tex, const char* drawn, const char* txt) { const NkRect b = { rx, ry, rightW, u.s(30) };
            u.Panel(b, NkCol::surface, NkCol::border, NkR::md * u.S);
            NkOwIco(u, tex, drawn, { b.x + u.s(10), b.y + u.s(8), u.s(14), u.s(14) }, NkCol::accent);
            u.TextV(b.x + u.s(32), b.y, u.s(30), txt, NkCol::foreground); ry += u.s(36); };
        int32 nK = 0, nD = 0; const char* const* kinds = NkWizKinds(&nK); const char* const* dials = NkWizDialects(&nD);
        char c1[120]; std::snprintf(c1, sizeof(c1), "%s - %s", (p.kind >= 0 && p.kind < nK) ? kinds[p.kind] : "consoleapp", (p.dialect >= 0 && p.dialect < nD) ? dials[p.dialect] : "C++20"); chip(NkWizKindTex(ic, p.kind), "terminal", c1);
        chip(ic.ouvrirDossier, "folder", p.location);
        { int32 dc = 0; const int32 nOs = NkWizOsCount(); for (int32 i = 0; i < nOs; ++i) if (p.os[i].on) ++dc;
          dc += (int32)p.cfgFlt.Size();    // filtres de configuration (Debug/Release/custom) comptent aussi
          dc += (int32)p.custFlt.Size();
          char c2[80]; std::snprintf(c2, sizeof(c2), "%d filtre(s) actif(s)", dc); chip(ic.platforms, "cpu", c2); }
        { char c3[80]; std::snprintf(c3, sizeof(c3), "%d dependance(s)", (int32)p.dependsOn.Size()); chip(ic.dependance, "link", c3); }
        return ((y > ry ? y : ry) - body.y) + u.s(20);
    }

    // ── Modale large « Nouveau / Editer projet » : 3 etapes internes (Definition / Filtres / Resume). ──
    inline void NkWizProjDialog(const NkUi& u, const NkRect& r, NkNewWsState* w, NkCodeDialogs* dlg, float32 dt, bool blockBg, const NkIcons& ic) {
        NkWizProject& p = w->projDraft;
        u.dl->AddRectFilled(r, NkScrim(160), 0.f);                       // voile
        const float32 mw = (r.w > u.s(960)) ? u.s(900) : (r.w - u.s(40));
        const float32 mh = (r.h > u.s(680)) ? u.s(640) : (r.h - u.s(30));
        const NkRect modal = { r.x + (r.w - mw) * 0.5f, r.y + (r.h - mh) * 0.5f, mw, mh };
        u.Panel(modal, NkCol::background, NkCol::border, NkR::lg * u.S);
        // En-tete
        NkOwIco(u, ic.nouveau, "plus-circle", { modal.x + u.s(22), modal.y + u.s(14), u.s(16), u.s(16) }, NkCol::primary);
        u.Text(modal.x + u.s(46), modal.y + u.s(15), w->projEditIdx >= 0 ? NkT("nws.editproj") : NkT("nws.newproj"), NkCol::foreground);
        { char st3[20]; std::snprintf(st3, sizeof(st3), "Etape %d sur 3", w->projStep + 1);
          u.Text(modal.x + mw - u.s(22) - u.TextW(st3), modal.y + u.s(15), st3, NkCol::mutedFg); }
        // Barre d'etapes
        const float32 sbH = u.s(84);
        const char* labels[] = { NkT("nws.step.definition"), NkT("nws.step.filters"), NkT("nws.step.summary") };
        u.Rect({ modal.x + u.s(16), modal.y + u.s(36), mw - u.s(32), 1.f }, NkCol::border);   // separateur sous l'en-tete
        NkWizSteps(u, { modal.x, modal.y + u.s(38), mw, sbH }, w->projStep, labels, 3, ic.valideSimple);
        // Corps defilable
        const float32 footH = u.s(54);
        const NkRect view = { modal.x, modal.y + u.s(38) + sbH, mw, mh - u.s(38) - sbH - footH };
        const NkRect bodyR = { view.x, view.y - w->projScroll, view.w, view.h };
        u.dl->PushClipRect(view, true);
        float32 contentH = view.h;
        w->focusClaimed = false;   // un champ revendiquera le clic ; sinon clic dans le vide = defocus
        switch (w->projStep) {
            case 0: contentH = NkProjDef(u, view, w, dlg, dt, blockBg, ic); break;     // colonnes a scroll independant
            case 1: contentH = NkProjFilters(u, view, w, dt, blockBg, ic); break;   // colonnes a scroll independant
            case 2: contentH = NkProjSummary(u, bodyR, w, dt, blockBg, ic); break;
            default: break;
        }
        u.dl->PopClipRect();
        // Clic dans le vide (aucun champ n'a pris le clic) -> on arrete l'edition / le clignotement.
        if (u.click && !blockBg && !w->focusClaimed && u.Hit(view)) { w->focus = -1; w->extractEditing = false; }
        // Molette de la modale APRES le contenu : un visualiseur de code survole l'a deja consommee.
        if (u.Hit(view) && u.ctx->input.wheel != 0.f && !blockBg) { w->projScroll -= u.ctx->input.wheel * u.s(40); u.ctx->input.wheel = 0.f;
            if (w->projScroll < 0.f) w->projScroll = 0.f; if (w->projScroll > w->projScrollMax) w->projScroll = w->projScrollMax; }
        w->projScrollMax = (contentH > view.h) ? (contentH - view.h) : 0.f;
        if (w->projScroll > w->projScrollMax) w->projScroll = w->projScrollMax;
        if (w->projScrollMax > 0.5f) {
            const float32 sw = u.s(10); const NkRect track = { view.x + view.w - sw - u.s(4), view.y, sw, view.h };
            u.dl->AddRectFilled(track, NkColor{ 18,21,26,160 }, sw * 0.5f);
            float32 thh = view.h * (view.h / (view.h + w->projScrollMax)); if (thh < u.s(28)) thh = u.s(28);
            const float32 ty = view.y + (view.h - thh) * (w->projScroll / w->projScrollMax);
            u.dl->AddRectFilled({ track.x + u.s(2), ty, sw - u.s(4), thh }, NkColor{ 70,76,84,255 }, (sw - u.s(4)) * 0.5f);
        }
        // Pied : Precedent / Annuler / Suivant|Creer
        const bool valid = p.name[0] && NkNewWsState::ValidName(p.name);
        const int32 fa = NkWizFooter(u, { modal.x, modal.y + mh - footH, mw, footH }, w->projStep, 2, blockBg, valid, NkT("nws.create.proj"), true, ic.valideSimple);
        if (fa == 1) { w->projDlg = false; w->focus = -1; }
        else if (fa == -1 && w->projStep > 0) { w->projStep--; w->focus = -1; w->projScroll = 0.f; w->codeSX = w->codeSY = 0.f; w->fltLSY = w->fltLSX = w->fltRSY = w->fltRSX = 0.f; w->cfgSY = w->cfgSX = 0.f; }
        else if (fa == 3 && valid) w->CommitProjDlg();                                                              // Creer maintenant (defauts)
        else if (fa == 2 && valid) { if (w->projStep < 2) { w->projStep++; w->focus = -1; w->projScroll = 0.f; w->codeSX = w->codeSY = 0.f; w->fltLSY = w->fltLSX = w->fltRSY = w->fltRSX = 0.f; w->cfgSY = w->cfgSX = 0.f; } else w->CommitProjDlg(); }
    }

    // ── ETAPE 3 — Toolchains : DETECTION REELLE + selection + assignation par plateforme. ──
    inline float32 NkWizStep3(const NkUi& u, const NkRect& body, NkNewWsState* w, float32 dt, bool blockBg, const NkIcons& ic) {
        (void)dt;
        if (!w->tcDetDone) w->DetectToolchains();   // detection unique (PATH + env + WSL2)
        else w->RefreshTcPtrs();                     // pointeurs toujours frais (anti-crash)
        const float32 padL = u.s(28);
        const float32 rightW = (body.w > u.s(760)) ? u.s(280) : u.s(240);
        const float32 cx = body.x + padL;
        const float32 rx = body.x + body.w - u.s(28) - rightW;
        const float32 cw = (rx - u.s(34)) - cx;
        const int32 nTc = (int32)w->tcDet.Size();
        // une plateforme a-t-elle au moins un toolchain detecte ciblant 'key' ?
        auto hasTarget = [&](const char* key) -> bool { for (int32 i = 0; i < nTc; ++i) if (w->tcDet[i].found && w->tcDet[i].target.Contains(key)) return true; return false; };

        // ===== COLONNE GAUCHE =====
        float32 y = body.y + u.s(20);
        // DETECTION AUTOMATIQUE
        NkWizLabel(u, cx, y, NkT("nws.autodetect")); y += u.s(22);
        { const NkRect box = { cx, y, cw, u.s(48) };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          if (NkWizCheck(u, box.x + u.s(14), box.y + u.s(24), "RegisterJengaGlobalToolchains() - Detection automatique au demarrage", w->tcAuto, blockBg)) w->tcAuto = !w->tcAuto;
          y += u.s(48) + u.s(20); }
        // TOOLCHAINS DETECTES (reels)
        { char hdr[80]; std::snprintf(hdr, sizeof(hdr), "TOOLCHAINS DETECTES SUR CE SYSTEME (%d)", nTc);
          NkWizLabel(u, cx, y, hdr); }
        { const NkRect rr = { cx + cw - u.s(80), y - u.s(4), u.s(80), u.s(22) }; const bool hv = !blockBg && u.Hit(rr);
          u.Rect(rr, hv ? NkCol::hover : NkCol::muted, NkR::sm * u.S);
          u.TextV(rr.x + u.s(10), rr.y, u.s(22), "Rescanner", hv ? NkCol::foreground : NkCol::mutedFg);
          if (hv && u.click) { w->tcDetDone = false; } }
        y += u.s(22);
        { const float32 rowH = u.s(42);
          const NkRect box = { cx, y, cw, u.s(10) + (nTc > 0 ? nTc : 1) * rowH };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          float32 ry2 = box.y + u.s(6);
          for (int32 i = 0; i < nTc; ++i) {
              const NkNewWsState::TcDet& t = w->tcDet[i];
              const NkColor tint = t.found ? NkCol::success : NkCol::accent;
              NkOwIco(u, t.found ? ic.valideSimple : 0u, t.found ? "check-circle" : "alert-triangle", { box.x + u.s(14), ry2 + u.s(13), u.s(16), u.s(16) }, tint);
              // Colonnes avec ellipsis pour ne JAMAIS deborder de la case (largeurs bornees, sans chevauchement).
              u.TextEllipsis(box.x + u.s(40), ry2 + u.s(5), cw * 0.36f - u.s(46), t.name.CStr(), t.found ? NkCol::foreground : NkCol::mutedFg);
              u.TextEllipsis(box.x + u.s(40), ry2 + u.s(22), cw * 0.36f - u.s(46), t.version.CStr(), NkCol::mutedFg);
              u.TextEllipsis(box.x + cw * 0.38f, ry2 + u.s(13), cw * 0.26f, t.target.CStr(), NkCol::mutedFg);
              u.TextEllipsis(box.x + cw * 0.66f, ry2 + u.s(13), cw * 0.32f - u.s(8), t.path.CStr(), t.found ? NkCol::mutedFg : NkCol::danger);
              ry2 += rowH;
              if (i + 1 < nTc) u.Rect({ box.x + u.s(10), ry2 - u.s(1), cw - u.s(20), 1.f }, NkCol::border);
          }
          y = box.y + box.h + u.s(20); }
        // TOOLCHAINS SELECTIONNES
        NkWizLabel(u, cx, y, "TOOLCHAINS SELECTIONNES POUR CE WORKSPACE"); y += u.s(22);
        { const int32 rows = (nTc + 2) / 3; const NkRect box = { cx, y, cw, u.s(16) + (rows > 0 ? rows : 1) * u.s(30) };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          const float32 colW = cw / 3.f;
          for (int32 i = 0; i < nTc; ++i) {
              const int32 col = i % 3, row = i / 3;
              const float32 bxx = box.x + u.s(14) + col * colW, byy = box.y + u.s(22) + row * u.s(30);
              if (w->tcDet[i].found) { if (NkWizCheck(u, bxx, byy, w->tcDet[i].name.CStr(), w->tcSel[i], blockBg)) w->tcSel[i] = !w->tcSel[i]; }
              else { NkWizCheck(u, bxx, byy, w->tcDet[i].name.CStr(), false, true); }   // indisponible -> grise
          }
          y = box.y + box.h + u.s(20); }
        // TOOLCHAINS PERSONNALISES
        NkWizLabel(u, cx, y, "TOOLCHAINS PERSONNALISES");
        { const float32 aw = u.s(150); const NkRect addR = { cx + cw - aw, y - u.s(4), aw, u.s(26) };
          const bool hv = !blockBg && u.Hit(addR);
          u.Rect(addR, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
          NkOwIco(u, ic.plus, "plus", { addR.x + u.s(10), addR.y + u.s(7), u.s(12), u.s(12) }, NkCol::foreground);
          u.TextV(addR.x + u.s(26), addR.y, u.s(26), "Ajouter manuel", NkCol::foreground);
          if (hv && u.click) { w->tcEditIdx = -1; w->tcDraft = NkNewWsState::CustomTc{}; w->tcDlg = true; w->tcDlgScroll = 0.f; w->tcDlgScrollX = 0.f; w->tcTested = false; w->tcTestScroll = 0.f; w->focus = -1; } }
        y += u.s(26);
        { const float32 rh = u.s(30); const float32 ch = u.s(14) + (w->customTc.Empty() ? u.s(22) : w->customTc.Size() * rh);
          const NkRect box = { cx, y, cw, ch };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          if (w->customTc.Empty()) u.TextV(box.x + u.s(14), box.y, ch, "(aucun toolchain custom defini)", NkCol::mutedFg);
          else { float32 dy = box.y + u.s(8); int32 rm = -1, ed = -1;
              for (usize i = 0; i < w->customTc.Size(); ++i) {
                  NkOwIco(u, ic.toolchains, "cpu", { box.x + u.s(12), dy + u.s(3), u.s(14), u.s(14) }, NkCol::accent);
                  u.TextEllipsis(box.x + u.s(34), dy, cw - u.s(90), w->customTc[i].name, NkCol::foreground);
                  // Bouton EDITER
                  const NkRect er = { box.x + cw - u.s(48), dy - u.s(3), u.s(18), u.s(18) }; const bool ehv = !blockBg && u.Hit(er);
                  NkOwIco(u, ic.editer, "edit", { er.x + u.s(3), er.y + u.s(3), u.s(13), u.s(13) }, ehv ? NkCol::primary : NkCol::mutedFg);
                  if (ehv && u.click) ed = (int32)i;
                  // Bouton SUPPRIMER
                  const NkRect xr = { box.x + cw - u.s(24), dy - u.s(3), u.s(18), u.s(18) }; const bool xhv = !blockBg && u.Hit(xr);
                  NkOwIco(u, ic.corbeille, "x", { xr.x + u.s(3), xr.y + u.s(3), u.s(13), u.s(13) }, xhv ? NkCol::danger : NkCol::mutedFg);
                  if (xhv && u.click) rm = (int32)i;
                  dy += rh;
              }
              if (ed >= 0) { w->tcEditIdx = ed; w->tcDraft = w->customTc[ed]; w->tcDlg = true; w->tcDlgScroll = 0.f; w->tcDlgScrollX = 0.f; w->tcTested = false; w->tcTestScroll = 0.f; w->focus = -1; }
              if (rm >= 0) w->customTc.Erase(w->customTc.Begin() + rm); }
          y += ch + u.s(22); }

        // ===== COLONNE DROITE : TOOLCHAIN PAR PLATEFORME =====
        float32 ry = body.y + u.s(20);
        NkWizLabel(u, rx, ry, "TOOLCHAIN PAR PLATEFORME"); ry += u.s(26);
        (void)hasTarget;
        // Chaque plateforme ne liste QUE les toolchains qui la ciblent (listes filtrees).
        auto rowTc = [&](const char* os, int32 platIdx, int32 id) {
            u.Text(rx, ry + u.s(8), os, NkCol::foreground);
            if (w->tcPlatN[platIdx] > 0) NkWizCombo(u, { rx + u.s(86), ry, rightW - u.s(86), u.s(30) }, id, w, w->tcPlatPtrs[platIdx], w->tcPlatN[platIdx], &w->tcPlatSel[platIdx], blockBg);
            else u.TextV(rx + u.s(86), ry, u.s(30), "Aucun toolchain detecte", NkCol::mutedFg);
            ry += u.s(38);
        };
        rowTc("Windows",   0, 800);
        rowTc("Linux",     1, 801);
        rowTc("macOS",     2, 802);
        rowTc("Android",   3, 803);
        rowTc("iOS",       4, 804);
        rowTc("Web",       5, 805);
        rowTc("HarmonyOS", 6, 806);
        ry += u.s(8);
        NkWizHint(u, rx, ry, rightW, "Genere usetoolchain(\"xxx\") dans with filter(\"system:X\")"); ry += u.s(24);

        return ((y > ry ? y : ry) - body.y) + u.s(20);
    }

    // Options du dialog toolchain.
    inline const char* const* NkWizTcFam(int32* n)  { static const char* k[] = { "Clang", "GCC", "MSVC", "NDK", "Emscripten", "Zig", "SDK", "OHOS-SDK (HarmonyOS)", "DevEco" }; if (n) *n = 9; return k; }
    inline const char* const* NkWizTcOsList(int32* n){ static const char* k[] = { "Windows", "Linux", "macOS", "Android", "iOS", "Web", "HarmonyOS" }; if (n) *n = 7; return k; }
    // Architectures / ABI CONFORMES au systeme choisi (os index = NkWizTcOsList : 0 Win,1 Linux,2 mac,3 Android,4 iOS,5 Web,6 HarmonyOS).
    inline const char* const* NkWizTcArchFor(int32 os, int32* n) {
        static const char* win[]  = { "x86_64", "ARM64", "x86" };
        static const char* lin[]  = { "x86_64", "ARM64", "x86", "ARM", "RISCV64" };
        static const char* mac[]  = { "ARM64 (Apple)", "x86_64", "Universal" };
        static const char* andr[] = { "arm64-v8a", "armeabi-v7a", "x86_64", "x86" };   // ABI Android
        static const char* ios[]  = { "ARM64", "ARM64 (sim)" };
        static const char* web[]  = { "WASM32", "WASM64" };
        static const char* ohos[] = { "arm64-v8a", "armeabi-v7a", "x86_64" };          // ABI HarmonyOS
        switch (os) { case 0: *n = 3; return win; case 1: *n = 5; return lin; case 2: *n = 3; return mac;
                      case 3: *n = 4; return andr; case 4: *n = 2; return ios; case 5: *n = 2; return web; case 6: *n = 3; return ohos; }
        *n = 3; return win;
    }
    // Environnement = ABI / runtime cible (dernier champ du triple LLVM) : libc, ABI, runtime.
    inline const char* const* NkWizTcEnvFor(int32 os, int32* n) {
        static const char* win[]  = { "msvc", "gnu (MinGW)" };
        static const char* lin[]  = { "gnu (glibc)", "musl" };
        static const char* mac[]  = { "darwin" };
        static const char* andr[] = { "android", "androideabi" };
        static const char* ios[]  = { "darwin", "simulator" };
        static const char* web[]  = { "emscripten", "wasi" };
        static const char* ohos[] = { "ohos", "musl" };
        switch (os) { case 0: *n = 2; return win; case 1: *n = 2; return lin; case 2: *n = 1; return mac;
                      case 3: *n = 2; return andr; case 4: *n = 2; return ios; case 5: *n = 2; return web; case 6: *n = 2; return ohos; }
        *n = 2; return lin;
    }

    // ── Dialog « Ajouter un Toolchain Manuel » (modale large, 2 colonnes). ──
    inline void NkWizTcDialog(const NkUi& u, const NkRect& r, NkNewWsState* w, NkCodeDialogs* dlg, float32 dt, bool blockBg, const NkIcons& ic) {
        NkNewWsState::CustomTc& t = w->tcDraft;
        u.dl->AddRectFilled(r, NkScrim(160), 0.f);
        const float32 mw = (r.w > u.s(900)) ? u.s(860) : (r.w - u.s(40));
        const float32 mh = (r.h > u.s(660)) ? u.s(620) : (r.h - u.s(30));
        const NkRect modal = { r.x + (r.w - mw) * 0.5f, r.y + (r.h - mh) * 0.5f, mw, mh };
        u.Panel(modal, NkCol::background, NkCol::border, NkR::lg * u.S);
        // En-tete
        const bool tcEdit = (w->tcEditIdx >= 0);
        NkOwIco(u, ic.toolchains, "cpu", { modal.x + u.s(22), modal.y + u.s(15), u.s(16), u.s(16) }, NkCol::primary);
        u.Text(modal.x + u.s(46), modal.y + u.s(16), tcEdit ? "EDITER LE TOOLCHAIN" : "AJOUTER UN TOOLCHAIN MANUEL", NkCol::foreground);
        { const NkRect xr = { modal.x + mw - u.s(30), modal.y + u.s(14), u.s(18), u.s(18) }; const bool xhv = u.Hit(xr);
          u.Icon("x", { xr.x + u.s(4), xr.y + u.s(4), u.s(11), u.s(11) }, xhv ? NkCol::danger : NkCol::mutedFg);
          if (xhv && u.click) { w->tcDlg = false; w->focus = -1; } }
        u.Rect({ modal.x + u.s(16), modal.y + u.s(40), mw - u.s(32), 1.f }, NkCol::border);
        const float32 footH = u.s(54);
        const NkRect view = { modal.x, modal.y + u.s(44), mw, mh - u.s(44) - footH };
        const bool bg = blockBg;   // déjà gere par le combo overlay

        const float32 rightW = u.s(230);
        const float32 rx = view.x + mw - u.s(20) - rightW, fH = u.s(28);
        const float32 sb = u.s(10);
        u.dl->PushClipRect(view, true);
        // ===== COLONNE GAUCHE (scroll V/H propre) =====
        const NkRect leftRect = { view.x + u.s(4), view.y + u.s(4), rx - view.x - u.s(12), view.h - u.s(8) };
        const float32 cx = leftRect.x + u.s(20) - w->tcDlgScrollX;
        const float32 cw = (leftRect.x + leftRect.w - u.s(10) - sb) - (leftRect.x + u.s(20));   // largeur contenu (fixe)
        const float32 leftTop = leftRect.y + u.s(12);
        u.dl->PushClipRect(leftRect, true);
        float32 y = leftTop - w->tcDlgScroll;
        int32 nF = 0, nO = 0; const char* const* FAM = NkWizTcFam(&nF); const char* const* OSL = NkWizTcOsList(&nO);
        NkWizLabel(u, cx, y, "IDENTITE"); y += u.s(20);
        u.Text(cx, y + u.s(7), "Nom", NkCol::mutedFg);
        NkWizField(u, { cx + u.s(110), y, cw - u.s(110), fH }, t.name, (int32)sizeof(t.name), 900, w, dt, bg, u.s(10)); y += fH + u.s(12);
        u.Text(cx, y + u.s(7), "Famille", NkCol::mutedFg);
        NkWizCombo(u, { cx + u.s(110), y, u.s(150), fH }, 910, w, FAM, nF, &t.family, bg);
        u.Text(cx + u.s(280), y + u.s(7), "Triple LLVM", NkCol::mutedFg);
        NkWizField(u, { cx + u.s(372), y, cw - u.s(372), fH }, t.triple, (int32)sizeof(t.triple), 901, w, dt, bg, u.s(10)); y += fH + u.s(24);
        NkWizLabel(u, cx, y, "CIBLE"); y += u.s(22);
        // Arch (ABI) et Env DEPENDENT du systeme choisi (ex: Android -> ABI arm64-v8a...). On reclampe a chaque changement.
        int32 nA = 0, nE = 0; const char* const* ARC = NkWizTcArchFor(t.os, &nA); const char* const* ENV = NkWizTcEnvFor(t.os, &nE);
        if (t.arch >= nA || t.arch < 0) t.arch = 0;
        if (t.env  >= nE || t.env  < 0) t.env  = 0;
        // Ligne 1 : Systeme + Arch/ABI.
        u.Text(cx, y + u.s(7), "Systeme", NkCol::mutedFg);
        NkWizCombo(u, { cx + u.s(110), y, u.s(160), fH }, 911, w, OSL, nO, &t.os, bg);
        u.Text(cx + u.s(290), y + u.s(7), (t.os == 3 || t.os == 6) ? "ABI" : "Arch", NkCol::mutedFg);
        NkWizCombo(u, { cx + u.s(340), y, u.s(170), fH }, 912, w, ARC, nA, &t.arch, bg); y += fH + u.s(10);
        // Ligne 2 : Env (cadre large) + explication.
        u.Text(cx, y + u.s(7), "Env", NkCol::mutedFg);
        NkWizCombo(u, { cx + u.s(110), y, u.s(220), fH }, 913, w, ENV, nE, &t.env, bg);
        u.TextEllipsis(cx + u.s(344), y + u.s(7), cw - u.s(344), "ABI/runtime (libc) - dernier champ du triple LLVM", NkCol::mutedFg);
        y += fH + u.s(24);
        u.Text(cx, y, "CHEMINS DES OUTILS", NkCol::mutedFg);
        u.Text(cx + u.TextW("CHEMINS DES OUTILS") + u.s(10), y, "- compilateurs = fichiers, sysroot = dossier", NkCol::mutedFg); y += u.s(22);
        auto pathRow = [&](const char* lab, char* buf, int32 cap, int32 id, bool pickDir) {
            u.Text(cx, y + u.s(7), lab, NkCol::mutedFg);
            NkWizField(u, { cx + u.s(130), y, cw - u.s(168), fH }, buf, cap, id, w, dt, bg, u.s(10));
            const NkRect br = { cx + cw - u.s(32), y, u.s(32), fH }; const bool hv = !bg && u.Hit(br);
            u.Rect(br, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
            NkOwIco(u, ic.ouvrirDossier, "folder", { br.x + u.s(9), br.y + u.s(8), u.s(14), u.s(14) }, NkCol::mutedFg);
            if (hv && u.click && dlg) { w->focus = -1; if (pickDir) dlg->BrowseInto(buf, cap, lab); else dlg->BrowseFile(buf, cap, lab); }
            y += fH + u.s(12);
        };
        pathRow("Compilateur C",   t.cc,      (int32)sizeof(t.cc),      902, false);
        pathRow("Compilateur C++", t.cxx,     (int32)sizeof(t.cxx),     903, false);
        pathRow("Archiver (ar)",   t.ar,      (int32)sizeof(t.ar),      904, false);
        pathRow("Linker",          t.ld,      (int32)sizeof(t.ld),      905, false);
        pathRow("Sysroot",         t.sysroot, (int32)sizeof(t.sysroot), 906, true);
        y += u.s(18);
        NkWizLabel(u, cx, y, "FLAGS SUPPLEMENTAIRES"); y += u.s(22);
        u.Text(cx, y + u.s(2), "cflags", NkCol::mutedFg);   NkProjPills(u, cx + u.s(90), y, cw - u.s(100), t.cflags,   900, w, dt, bg, ic.plus); y += NkProjPillsH(u, cw - u.s(100), t.cflags, w->projFltTarget == 900) + u.s(6);
        u.Text(cx, y + u.s(2), "cxxflags", NkCol::mutedFg); NkProjPills(u, cx + u.s(90), y, cw - u.s(100), t.cxxflags, 901, w, dt, bg, ic.plus); y += NkProjPillsH(u, cw - u.s(100), t.cxxflags, w->projFltTarget == 901) + u.s(6);
        u.Text(cx, y + u.s(2), "ldflags", NkCol::mutedFg);  NkProjPills(u, cx + u.s(90), y, cw - u.s(100), t.ldflags,  902, w, dt, bg, ic.plus); y += NkProjPillsH(u, cw - u.s(100), t.ldflags, w->projFltTarget == 902) + u.s(6);
        // Fin colonne gauche : clip + molette + scrollbars V/H.
        const float32 leftContentH = (y + w->tcDlgScroll) - leftTop + u.s(16);
        u.dl->PopClipRect();
        NkWizColScroll(u, leftRect, leftContentH, cw + u.s(40), w->tcDlgScroll, w->tcDlgScrollX, bg);

        // ===== COLONNE DROITE =====
        float32 ry = view.y + u.s(14);
        NkWizLabel(u, rx, ry, "TESTER"); ry += u.s(20);
        { const NkRect br = { rx, ry, rightW, u.s(32) }; const bool hv = !bg && u.Hit(br);
          u.Rect(br, hv ? NkColHover(NkCol::primary) : NkCol::primary, NkR::md * u.S);
          NkOwIco(u, ic.search, "search", { br.x + u.s(14), br.y + u.s(9), u.s(14), u.s(14) }, NkCol::primaryFg);
          u.TextV(br.x + u.s(36), br.y, u.s(32), "Lancer la verification", NkCol::primaryFg);
          if (hv && u.click) w->tcTested = true; ry += u.s(40); }
        { const NkRect box = { rx, ry, rightW, u.s(100) }; u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          if (!w->tcTested) { u.TextV(box.x + u.s(12), box.y, u.s(100), "(cliquez sur Lancer la verification)", NkCol::mutedFg); }
          else {
              struct Chk { const char* lab; const char* path; bool dir; };
              const Chk items[] = { { "Compilateur C", t.cc, false }, { "Compilateur C++", t.cxx, false }, { "Archiver", t.ar, false }, { "Linker", t.ld, false }, { "Sysroot", t.sysroot, true } };
              const float32 rowH = u.s(21), sbW = u.s(8);
              const float32 contentH = 5 * rowH + u.s(10);
              const float32 maxS = (contentH > box.h) ? contentH - box.h : 0.f;
              if (!bg && u.Hit(box) && u.ctx->input.wheel != 0.f) { w->tcTestScroll -= u.ctx->input.wheel * u.s(30); u.ctx->input.wheel = 0.f; }
              if (w->tcTestScroll < 0.f) w->tcTestScroll = 0.f; if (w->tcTestScroll > maxS) w->tcTestScroll = maxS;
              u.dl->PushClipRect(box, true);
              float32 dy = box.y + u.s(8) - w->tcTestScroll;
              for (const Chk& c : items) {
                  char ln[200];
                  if (!c.path || !c.path[0]) { NkOwIco(u, 0u, "minus", { box.x + u.s(12), dy, u.s(13), u.s(13) }, NkCol::mutedFg);
                      std::snprintf(ln, sizeof(ln), "%s : non renseigne", c.lab); u.TextEllipsis(box.x + u.s(32), dy - u.s(2), rightW - u.s(46) - sbW, ln, NkCol::mutedFg); }
                  else { const bool ok = c.dir ? NkDirectory::Exists(c.path) : NkFile::Exists(c.path);
                      NkOwIco(u, ok ? ic.valideSimple : 0u, ok ? "check-circle" : "x", { box.x + u.s(12), dy, u.s(13), u.s(13) }, ok ? NkCol::success : NkCol::danger);
                      std::snprintf(ln, sizeof(ln), "%s : %s", c.lab, ok ? "trouve" : "introuvable"); u.TextEllipsis(box.x + u.s(32), dy - u.s(2), rightW - u.s(46) - sbW, ln, ok ? NkCol::foreground : NkCol::danger); }
                  dy += rowH;
              }
              u.dl->PopClipRect();
              if (maxS > 0.5f) { const NkRect tr = { box.x + box.w - sbW - u.s(2), box.y + u.s(2), sbW, box.h - u.s(4) };
                  u.dl->AddRectFilled(tr, NkColor{ 18,21,26,150 }, sbW * 0.5f);
                  float32 th = box.h * (box.h / contentH); if (th < u.s(20)) th = u.s(20);
                  const float32 tt = tr.y + (tr.h - th) * (w->tcTestScroll / maxS);
                  u.dl->AddRectFilled({ tr.x + u.s(1), tt, sbW - u.s(2), th }, NkColor{ 70,76,84,255 }, sbW * 0.5f); }
          }
          ry += u.s(108); }
        NkWizLabel(u, rx, ry, "ENREGISTREMENT"); ry += u.s(20);
        { const NkRect box = { rx, ry, rightW, u.s(80) }; u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          if (NkWizCheck(u, box.x + u.s(12), box.y + u.s(16), "Registre global", t.regGlobal, bg)) t.regGlobal = !t.regGlobal;
          u.TextEllipsis(box.x + u.s(34), box.y + u.s(32), rightW - u.s(44), "~/.jenga/toolchains_registry.json", NkCol::mutedFg);
          // Consequence du registre global : disponible dans tous les workspaces.
          NkOwIco(u, t.regGlobal ? ic.valideSimple : 0u, t.regGlobal ? "check" : "minus", { box.x + u.s(14), box.y + u.s(54), u.s(13), u.s(13) }, t.regGlobal ? NkCol::success : NkCol::mutedFg);
          u.TextEllipsis(box.x + u.s(34), box.y + u.s(52), rightW - u.s(44), "Disponible dans tous les workspaces", t.regGlobal ? NkCol::foreground : NkCol::mutedFg);
          ry += u.s(88); }
        NkWizLabel(u, rx, ry, "APERCU .JENGA"); ry += u.s(20);
        { const NkRect box = { rx, ry, rightW, u.s(70) }; u.Panel(box, NkCol::input, NkCol::border, NkR::md * u.S);
          char l1[120]; std::snprintf(l1, sizeof(l1), "with filter(\"system:%s\"):", (t.os >= 0 && t.os < nO) ? OSL[t.os] : "Linux");
          char l2[120]; std::snprintf(l2, sizeof(l2), "    usetoolchain(\"%s\")", t.name);
          NkCodeDrawLine(u, box.x + u.s(10), box.y + u.s(10), NkString(l1));
          NkCodeDrawLine(u, box.x + u.s(10), box.y + u.s(30), NkString(l2)); ry += u.s(78); }
        u.dl->PopClipRect();

        // Pied
        u.Rect({ modal.x, modal.y + mh - footH, mw, footH }, NkCol::sidebar);
        u.Rect({ modal.x, modal.y + mh - footH, mw, 1.f }, NkCol::border);
        const float32 by = modal.y + mh - footH + (footH - u.s(34)) * 0.5f;
        const bool valid = t.name[0] != 0;
        const char* okLbl = tcEdit ? "Enregistrer" : "Ajouter le Toolchain";
        { const float32 bw = u.TextW(okLbl) + u.s(50); const NkRect ar2 = { modal.x + mw - u.s(20) - bw, by, bw, u.s(34) };
          const bool hv = valid && !bg && u.Hit(ar2);
          u.Rect(ar2, !valid ? NkCol::muted : (hv ? NkColHover(NkCol::primary) : NkCol::primary), NkR::md * u.S);
          const NkColor fg = valid ? NkCol::primaryFg : NkCol::mutedFg;
          NkOwIco(u, ic.valideSimple, "check", { ar2.x + u.s(16), ar2.y + u.s(10), u.s(14), u.s(14) }, fg);
          u.TextV(ar2.x + u.s(36), ar2.y, u.s(34), okLbl, fg);
          if (hv && u.click) {
              if (tcEdit && w->tcEditIdx < (int32)w->customTc.Size()) w->customTc[w->tcEditIdx] = t;   // remplace l'existant
              else w->customTc.PushBack(t);                                                            // nouveau
              w->tcDlg = false; w->tcEditIdx = -1; w->focus = -1;
          } }
        { const float32 bw = u.s(96); const NkRect cr = { modal.x + mw - u.s(20) - (u.TextW(okLbl) + u.s(50)) - u.s(8) - bw, by, bw, u.s(34) };
          if (u.Button(cr, "Annuler", NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S) && !bg) { w->tcDlg = false; w->tcEditIdx = -1; w->focus = -1; } }
    }

    // ── ETAPE 4 — Plateformes & Architectures ──
    inline const char* const* NkWizApiLvl(int32* n) { static const char* k[] = { "21", "23", "24", "26", "28", "30", "31", "33", "34", "35" }; if (n) *n = 10; return k; }
    inline const char* const* NkWizEmMem(int32* n)  { static const char* k[] = { "16 MB", "32 MB", "64 MB", "128 MB", "256 MB" }; if (n) *n = 5; return k; }
    inline const char* const* NkWizEmStack(int32* n){ static const char* k[] = { "1 MB", "2 MB", "4 MB", "8 MB", "16 MB" }; if (n) *n = 5; return k; }

    inline float32 NkWizStep4(const NkUi& u, const NkRect& body, NkNewWsState* w, float32 dt, bool blockBg, const NkIcons& ic) {
        (void)dt;
        if (w->tcDetDone) w->RefreshTcPtrs();   // pointeurs frais (le statut toolchain par OS s'appuie dessus)
        const float32 padL = u.s(28);
        const float32 rightW = (body.w > u.s(760)) ? u.s(280) : u.s(240);
        const float32 cx = body.x + padL;
        const float32 rx = body.x + body.w - u.s(28) - rightW;
        const float32 cw = (rx - u.s(34)) - cx;

        // Table des OS cibles : nom, enum, categorie (0 Desktop,1 Mobile,2 Web,3 Consoles), plateforme de detection, flag, message si indispo, icone (1 warn / 2 lock).
        struct OsT { const char* name; const char* en; int32 cat; int32 plat; bool* flag; const char* req; int32 ri; };
        const OsT oss[] = {
            { "Windows", "TargetOS.WINDOWS", 0, 0, &w->osWin, nullptr, 0 },
            { "Linux", "TargetOS.LINUX", 0, 1, &w->osLinux, nullptr, 0 },
            { "macOS", "TargetOS.MACOS", 0, 2, &w->osMac, "Requiert macOS hote", 1 },
            { "FreeBSD", "TargetOS.FREEBSD", 0, -1, &w->osFreeBSD, "Non configure", 1 },
            { "Android", "TargetOS.ANDROID", 1, 3, &w->osAndroid, nullptr, 0 },
            { "iOS", "TargetOS.IOS", 1, 4, &w->osIos, "Requiert macOS hote", 1 },
            { "tvOS", "TargetOS.TVOS", 1, -1, &w->osTvos, "Requiert macOS hote", 1 },
            { "watchOS", "TargetOS.WATCHOS", 1, -1, &w->osWatchos, "Requiert macOS hote", 1 },
            { "HarmonyOS", "TargetOS.HARMONYOS", 1, 6, &w->osHarmony, "NDK OHOS requis", 1 },
            { "Web/WASM", "TargetOS.WEB", 2, 5, &w->osWeb, nullptr, 0 },
            { "Xbox One", "TargetOS.XBOX_ONE", 3, -1, &w->osXboxOne, "GDK requis", 1 },
            { "Xbox Series", "TargetOS.XBOX_SERIES", 3, -1, &w->osXboxSeries, "GDK requis", 1 },
            { "PS4", "TargetOS.PS4", 3, -1, &w->osPs4, "SDK Sony requis", 2 },
            { "PS5", "TargetOS.PS5", 3, -1, &w->osPs5, "SDK Sony requis", 2 },
            { "Switch", "TargetOS.SWITCH", 3, -1, &w->osSwitch, "SDK Nintendo requis", 2 },
        };
        const int32 nOss = (int32)(sizeof(oss) / sizeof(oss[0]));
        const char* cats[] = { "Desktop", "Mobile", "Web", "Consoles" };

        // ===== COLONNE GAUCHE =====
        float32 y = body.y + u.s(20);
        NkWizLabel(u, cx, y, "SYSTEMES D'EXPLOITATION CIBLES (targetoses)"); y += u.s(22);
        { const float32 headerH = u.s(26), rowH = u.s(28);
          const float32 boxH = u.s(10) + 4 * headerH + nOss * rowH + u.s(10);
          const NkRect box = { cx, y, cw, boxH };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          float32 by = box.y + u.s(8);
          for (int32 cat = 0; cat < 4; ++cat) {
              // bandeau de categorie
              u.dl->AddRectFilled({ box.x + u.s(4), by, cw - u.s(8), headerH - u.s(4) }, NkCol::muted, NkR::sm * u.S);
              u.TextV(box.x + u.s(14), by - u.s(2), headerH, cats[cat], NkCol::accent);
              by += headerH;
              for (int32 i = 0; i < nOss; ++i) {
                  const OsT& o = oss[i]; if (o.cat != cat) continue;
                  const bool hasTc = (o.plat >= 0 && w->tcPlatN[o.plat] > 0);
                  const bool avail = (o.req == nullptr) || hasTc;
                  // checkbox (grise si indisponible)
                  if (avail) { if (NkWizCheck(u, box.x + u.s(16), by + u.s(13), o.name, *o.flag, blockBg)) *o.flag = !*o.flag; }
                  else { *o.flag = false; NkWizCheck(u, box.x + u.s(16), by + u.s(13), o.name, false, true); }
                  // enum cible
                  u.Text(box.x + u.s(180), by + u.s(7), o.en, NkCol::mutedFg);
                  // statut a droite
                  if (hasTc) { NkOwIco(u, ic.valideSimple, "check-circle", { box.x + cw - u.s(190), by + u.s(7), u.s(12), u.s(12) }, NkCol::success);
                      char st[80]; std::snprintf(st, sizeof(st), "Toolchain: %s", w->tcPlatPtrs[o.plat][0]);
                      u.TextEllipsis(box.x + cw - u.s(174), by + u.s(6), u.s(166), st, NkCol::success); }
                  else { NkOwIco(u, o.ri == 2 ? ic.lock : 0u, o.ri == 2 ? "lock" : "alert-triangle", { box.x + cw - u.s(190), by + u.s(7), u.s(12), u.s(12) }, o.ri == 2 ? NkCol::mutedFg : NkCol::accent);
                      u.TextEllipsis(box.x + cw - u.s(174), by + u.s(6), u.s(166), o.req ? o.req : "Non configure", NkCol::mutedFg); }
                  by += rowH;
              }
          }
          y = box.y + boxH + u.s(22); }

        // ARCHITECTURES CIBLES (grille 3 colonnes)
        NkWizLabel(u, cx, y, "ARCHITECTURES CIBLES (targetarchs)"); y += u.s(22);
        { struct ArchT { const char* name; const char* en; bool* flag; };
          const ArchT archs[] = {
              { "x86_64", "TargetArch.X86_64", &w->archX64 }, { "ARM64", "TargetArch.ARM64", &w->archArm64 }, { "WASM32", "TargetArch.WASM32", &w->archWasm32 },
              { "x86", "TargetArch.X86", &w->archX86 }, { "ARM", "TargetArch.ARM", &w->archArm }, { "RISCV64", "TargetArch.RISCV64", &w->archRiscv64 },
              { "WASM64", "TargetArch.WASM64", &w->archWasm64 }, { "MIPS", "TargetArch.MIPS", &w->archMips },
          };
          const int32 nArch = (int32)(sizeof(archs) / sizeof(archs[0]));
          const int32 cols = 3; const int32 rows = (nArch + cols - 1) / cols;
          const float32 colW = cw / cols, rowH = u.s(40);
          const NkRect box = { cx, y, cw, u.s(12) + rows * rowH };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          for (int32 i = 0; i < nArch; ++i) {
              const int32 col = i % cols, row = i / cols;
              const float32 ax = box.x + u.s(14) + col * colW, ay = box.y + u.s(14) + row * rowH;
              if (NkWizCheck(u, ax, ay, archs[i].name, *archs[i].flag, blockBg)) *archs[i].flag = !*archs[i].flag;
              u.Text(ax + u.s(28), ay + u.s(12), archs[i].en, NkCol::mutedFg);
          }
          y = box.y + box.h + u.s(20); }

        // ===== COLONNE DROITE =====
        float32 ry = body.y + u.s(20);
        // ABIS ANDROID (si Android coche)
        NkWizLabel(u, rx, ry, "ABIS ANDROID"); ry += u.s(22);
        { const bool on = w->osAndroid;
          const NkRect box = { rx, ry, rightW, u.s(168) };
          u.Panel(box, on ? NkCol::surface : NkCol::background, NkCol::border, NkR::md * u.S);
          const bool dis = !on || blockBg;
          if (NkWizCheck(u, box.x + u.s(14), box.y + u.s(18), "arm64-v8a", w->abiArm64v8a, dis)) w->abiArm64v8a = !w->abiArm64v8a;
          if (NkWizCheck(u, box.x + u.s(140), box.y + u.s(18), "x86_64", w->abiX86_64, dis)) w->abiX86_64 = !w->abiX86_64;
          if (NkWizCheck(u, box.x + u.s(14), box.y + u.s(42), "armeabi-v7a", w->abiArmeabi, dis)) w->abiArmeabi = !w->abiArmeabi;
          if (NkWizCheck(u, box.x + u.s(140), box.y + u.s(42), "x86", w->abiX86, dis)) w->abiX86 = !w->abiX86;
          int32 nApi = 0; const char* const* API = NkWizApiLvl(&nApi);
          u.Text(box.x + u.s(14), box.y + u.s(66), "API Min", NkCol::mutedFg);   NkWizCombo(u, { box.x + rightW - u.s(86), box.y + u.s(62), u.s(72), u.s(26) }, 820, w, API, nApi, &w->androidApiMin, dis);
          u.Text(box.x + u.s(14), box.y + u.s(94), "API Target", NkCol::mutedFg); NkWizCombo(u, { box.x + rightW - u.s(86), box.y + u.s(90), u.s(72), u.s(26) }, 821, w, API, nApi, &w->androidApiTarget, dis);
          u.Text(box.x + u.s(14), box.y + u.s(122), "Compile SDK", NkCol::mutedFg); NkWizCombo(u, { box.x + rightW - u.s(86), box.y + u.s(118), u.s(72), u.s(26) }, 822, w, API, nApi, &w->androidCompileSdk, dis);
          u.Text(box.x + u.s(14), box.y + u.s(146), "App ID", NkCol::mutedFg);     NkWizField(u, { box.x + u.s(72), box.y + u.s(142), rightW - u.s(86), u.s(26) }, w->androidAppId, (int32)sizeof(w->androidAppId), 150, w, dt, dis, u.s(8));
          ry += u.s(176); }
        // OPTIONS EMSCRIPTEN (si Web coche)
        NkWizLabel(u, rx, ry, "OPTIONS EMSCRIPTEN (Web)"); ry += u.s(22);
        { const bool on = w->osWeb;
          const NkRect box = { rx, ry, rightW, u.s(102) };
          u.Panel(box, on ? NkCol::surface : NkCol::background, NkCol::border, NkR::md * u.S);
          const bool dis = !on || blockBg;
          int32 nM = 0, nS = 0; const char* const* MEM = NkWizEmMem(&nM); const char* const* STK = NkWizEmStack(&nS);
          u.Text(box.x + u.s(14), box.y + u.s(18), "Initial Memory", NkCol::mutedFg); NkWizCombo(u, { box.x + rightW - u.s(96), box.y + u.s(14), u.s(82), u.s(26) }, 823, w, MEM, nM, &w->emInitMem, dis);
          u.Text(box.x + u.s(14), box.y + u.s(46), "Stack Size", NkCol::mutedFg);     NkWizCombo(u, { box.x + rightW - u.s(96), box.y + u.s(42), u.s(82), u.s(26) }, 824, w, STK, nS, &w->emStackSize, dis);
          u.Text(box.x + u.s(14), box.y + u.s(74), "Export Name", NkCol::mutedFg);    NkWizField(u, { box.x + u.s(96), box.y + u.s(70), rightW - u.s(110), u.s(26) }, w->emExportName, (int32)sizeof(w->emExportName), 151, w, dt, dis, u.s(8));
          ry += u.s(110); }

        return ((y > ry ? y : ry) - body.y) + u.s(20);
    }

    // ── ETAPE 5 — Resume & Generation ──
    inline float32 NkWizStep5(const NkUi& u, const NkRect& body, NkNewWsState* w, float32 dt, bool blockBg, const NkIcons& ic) {
        if (w->tcDetDone) w->RefreshTcPtrs();   // BuildJenga lit tcPlatPtrs -> on garantit des pointeurs valides (anti-crash)
        const float32 cx = body.x + u.s(28);
        const float32 cw = body.w - u.s(56);
        float32 y = body.y + u.s(20);
        NkWizLabel(u, cx, y, "RESUME - APERCU DU .JENGA GENERE");
        // Projets en fichier .jenga SEPARE -> un onglet chacun (+ l'onglet workspace).
        NkVector<int32> sepProj;
        for (usize i = 0; i < w->projects.Size(); ++i) if (w->projects[i].separateFile) sepProj.PushBack((int32)i);
        const int32 nTabs = 1 + (int32)sepProj.Size();
        if (w->wsTab < 0 || w->wsTab >= nTabs) w->wsTab = 0;
        const bool isWs = (w->wsTab == 0);
        // Bouton Copier (toujours) + Editer (onglet workspace uniquement).
        { const char* cl = "Copier"; const float32 cwd = u.TextW(cl) + u.s(34);
          const NkRect cr = { cx + cw - cwd, y - u.s(4), cwd, u.s(24) }; const bool chv = !blockBg && u.Hit(cr);
          u.Rect(cr, chv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
          NkOwIco(u, ic.fileCode, "copy", { cr.x + u.s(10), cr.y + u.s(5), u.s(13), u.s(13) }, NkCol::foreground);
          u.TextV(cr.x + u.s(26), cr.y, u.s(24), cl, NkCol::foreground);
          NkString curCode = isWs ? (w->wsExtractEdited ? w->wsExtractBuf : w->BuildJenga()) : w->BuildProjectFile(w->projects[sepProj[w->wsTab - 1]]);
          if (chv && u.click) u.ctx->SetClipboard(curCode.CStr());
          if (isWs) { const char* el = w->wsExtractEditing ? "Terminer" : "Editer manuellement"; const float32 ew = u.TextW(el) + u.s(34);
              const NkRect er = { cx + cw - cwd - u.s(8) - ew, y - u.s(4), ew, u.s(24) }; const bool ehv = !blockBg && u.Hit(er);
              u.Rect(er, w->wsExtractEditing ? NkCol::success : (ehv ? NkCol::hover : NkCol::muted), NkR::md * u.S);
              NkOwIco(u, w->wsExtractEditing ? ic.valideSimple : ic.editer, w->wsExtractEditing ? "check" : "edit", { er.x + u.s(10), er.y + u.s(5), u.s(13), u.s(13) }, w->wsExtractEditing ? NkCol::primaryFg : NkCol::foreground);
              u.TextV(er.x + u.s(26), er.y, u.s(24), el, w->wsExtractEditing ? NkCol::primaryFg : NkCol::foreground);
              if (ehv && u.click) { w->focusClaimed = true; if (!w->wsExtractEditing) { w->wsExtractBuf = w->BuildJenga(); w->wsExtractCaret = 0; w->wsExtractEditing = true; w->wsExtractEdited = true; w->codeSX = w->codeSY = 0.f; } else w->wsExtractEditing = false; } } }
        y += u.s(24);
        // Barre d'onglets (seulement s'il y a des projets separes).
        if (nTabs > 1) {
            float32 tx = cx;
            for (int32 t = 0; t < nTabs; ++t) {
                char lab[100]; if (t == 0) std::snprintf(lab, sizeof(lab), "%s", w->jengaFile);
                else std::snprintf(lab, sizeof(lab), "%s.jenga", w->projects[sepProj[t - 1]].name);
                const float32 tw = u.TextW(lab) + u.s(28); const NkRect tr = { tx, y, tw, u.s(28) };
                const bool act = (w->wsTab == t); const bool hv = !blockBg && u.Hit(tr);
                u.Panel(tr, act ? NkCol::secondary : (hv ? NkCol::hover : NkCol::surface), act ? NkCol::primary : NkCol::border, NkR::sm * u.S);
                NkOwIco(u, t == 0 ? ic.workspace : ic.fileCode, t == 0 ? "package" : "file", { tr.x + u.s(8), tr.y + u.s(8), u.s(12), u.s(12) }, act ? NkCol::primary : NkCol::mutedFg);
                u.TextEllipsis(tr.x + u.s(24), tr.y + u.s(7), tw - u.s(30), lab, act ? NkCol::primary : NkCol::foreground);
                if (hv && u.click && w->wsTab != t) { w->wsTab = t; w->wsExtractEditing = false; w->codeSX = w->codeSY = 0.f; }
                tx += tw + u.s(6);
            }
            y += u.s(34);
        }
        // Apercu / edition du fichier de l'onglet actif.
        const float32 codeH = u.s(330);
        if (isWs && w->wsExtractEditing) {
            const bool moved = NkWizCodeEditInput(u, w->wsExtractBuf, w->wsExtractCaret, blockBg);
            w->blink += dt; if (w->blink > 1.f) w->blink -= 1.f;
            NkWizCodeBox(u, { cx, y, cw, codeH }, w->wsExtractBuf, w, blockBg, true, &w->wsExtractCaret, w->blink, moved);
        } else {
            const NkString code = isWs ? (w->wsExtractEdited ? w->wsExtractBuf : w->BuildJenga())
                                       : w->BuildProjectFile(w->projects[sepProj[w->wsTab - 1]]);
            NkWizCodeBox(u, { cx, y, cw, codeH }, code, w, blockBg, true);
        }
        y += codeH + u.s(20);

        // CE QUI SERA CREE
        NkWizLabel(u, cx, y, "CE QUI SERA CREE"); y += u.s(22);
        const NkString folder = w->FinalFolder();
        auto fileLine = [&](const NkString& path) {
            NkOwIco(u, ic.valideSimple, "check-circle", { cx, y + u.s(1), u.s(14), u.s(14) }, NkCol::success);
            u.TextEllipsis(cx + u.s(24), y, cw - u.s(28), path.CStr(), NkCol::foreground); y += u.s(24);
        };
        { NkString f = folder; f += "/"; fileLine(f); }
        { NkString f = folder; f += "/"; f += w->jengaFile; fileLine(f); }
        for (usize i = 0; i < w->projects.Size(); ++i) {
            const NkWizProject& p = w->projects[i];
            if (p.separateFile) { NkString f = folder; f += "/"; f += p.location; f += p.name; f += ".jenga"; fileLine(f); }
            if (p.genMain && (p.kind == 0 || p.kind == 1)) { NkString f = folder; f += "/"; f += p.location; f += "src/main.cpp"; fileLine(f); }
            if (p.kind == 2 || p.kind == 3) { NkString f = folder; f += "/"; f += p.location; f += "include/"; fileLine(f); }
            if (p.genReadme) { NkString f = folder; f += "/"; f += p.location; f += "README.md"; fileLine(f); }
        }
        if (w->gitIgnore) { NkString f = folder; f += "/.gitignore"; fileLine(f); }
        if (w->gitInit)   { NkString f = folder; f += "/.git/ (init)"; fileLine(f); }
        y += u.s(8);
        return (y - body.y) + u.s(20);
    }

    // ── Page titree generique (etape 5 a etoffer) ──
    inline void NkWizPlaceholder(const NkUi& u, const NkRect& body, const char* title, const char* sub) {
        const float32 cx = body.x + body.w * 0.5f, cy = body.y + body.h * 0.5f;
        u.Icon("layers", { cx - u.s(18), cy - u.s(46), u.s(36), u.s(36) }, NkCol::muted);
        const float32 tw = u.TextW(title);
        u.Text(cx - tw * 0.5f, cy, title, NkCol::foreground);
        const float32 sw = u.TextW(sub);
        u.Text(cx - sw * 0.5f, cy + u.s(22), sub, NkCol::mutedFg);
    }

    // ── Panneau wizard « Nouveau Workspace ». Renvoie 1 si Annuler (retour Accueil). ──
    inline int32 NkNewWsPanel(const NkUi& u, const NkRect& r, NkNewWsState* w, NkCodeState* st,
                              NkCodeDialogs* dlg, float32 dt, const NkIcons& ic) {
        (void)dlg;
        w->EnsureInit(st);
        if (w->focus == 3) w->jengaManual = true;   // l'utilisateur edite le nom de fichier
        w->SyncDerived();
        // Un combo ouvert OU le picker de dossier modal capturent les evenements du fond.
        // Un combo, le picker modal OU le dialog projet gelent la page + le pied.
        const bool blockBg = (w->comboOpen >= 0) || (dlg && dlg->pickerOpen) || w->projDlg || w->tcDlg || NkTxtMenu().open;
        w->focusClaimed = false;   // reinitialise : un champ posera true s'il capte le clic (sinon clic vide = defocus)

        u.Rect(r, NkCol::background);

        // ── En-tete ──
        const float32 hH = u.s(44);
        u.Rect({ r.x, r.y, r.w, hH }, NkCol::background);
        NkOwIco(u, ic.nouveau, "plus-circle", { r.x + u.s(28), r.y + (hH - u.s(18)) * 0.5f, u.s(18), u.s(18) }, NkCol::primary);
        u.Text(r.x + u.s(54), r.y + (hH - u.Lh()) * 0.5f, NkT("nws.title"), NkCol::foreground);
        { char st5[24]; std::snprintf(st5, sizeof(st5), "Etape %d sur 5", w->step + 1);
          const float32 tw = u.TextW(st5);
          u.Text(r.x + r.w - u.s(28) - tw, r.y + (hH - u.Lh()) * 0.5f, st5, NkCol::mutedFg); }

        // ── Barre d'etapes ──
        const float32 sbH = u.s(86);
        const char* labels[] = { NkT("nws.step.ws"), NkT("nws.step.projects"), NkT("nav.toolchains"), NkT("nav.platforms"), NkT("nws.step.summary") };
        NkWizSteps(u, { r.x, r.y + hH, r.w, sbH }, w->step, labels, 5, ic.valideSimple);

        // ── Corps (defilable verticalement) ──
        const float32 footH = u.s(56);
        const NkRect view = { r.x, r.y + hH + sbH, r.w, r.h - hH - sbH - footH };
        // Largeur de contenu minimale (sinon scroll horizontal) : les 2 colonnes + marges.
        const float32 minW = u.s(860);
        const float32 bodyW = (view.w < minW) ? minW : view.w;
        w->hscrollMax = (bodyW > view.w) ? (bodyW - view.w) : 0.f;
        if (w->hscroll > w->hscrollMax) w->hscroll = w->hscrollMax; if (w->hscroll < 0.f) w->hscroll = 0.f;
        const NkRect body = { view.x - w->hscroll, view.y - w->scroll, bodyW, view.h };
        u.dl->PushClipRect(view, true);
        float32 contentH = view.h;
        switch (w->step) {
            case 0: contentH = NkWizStep1(u, body, w, dlg, dt, blockBg, ic); break;
            case 1: contentH = NkWizStep2(u, body, w, dt, blockBg, ic); break;
            case 2: contentH = NkWizStep3(u, body, w, dt, blockBg, ic); break;
            case 3: contentH = NkWizStep4(u, body, w, dt, blockBg, ic); break;
            case 4: contentH = NkWizStep5(u, body, w, dt, blockBg, ic); break;
            default: break;
        }
        u.dl->PopClipRect();
        // Molette APRES le contenu : un visualiseur de code survole l'a deja consommee (sinon le scroll global la mange).
        if (u.Hit(view) && u.ctx->input.wheel != 0.f && !blockBg) { w->scroll -= u.ctx->input.wheel * u.s(40); u.ctx->input.wheel = 0.f; }
        w->scrollMax = (contentH > view.h) ? (contentH - view.h) : 0.f;
        if (w->scroll < 0.f) w->scroll = 0.f; if (w->scroll > w->scrollMax) w->scroll = w->scrollMax;
        // Scrollbar verticale.
        if (w->scrollMax > 0.5f) {
            const float32 sw = u.s(10);
            const NkRect track = { view.x + view.w - sw - u.s(2), view.y, sw, view.h };
            u.dl->AddRectFilled(track, NkColor{ 18,21,26,160 }, sw * 0.5f);
            float32 thh = view.h * (view.h / (view.h + w->scrollMax)); if (thh < u.s(28)) thh = u.s(28);
            const float32 ty = view.y + (view.h - thh) * (w->scroll / w->scrollMax);
            const NkRect thumb = { track.x + u.s(2), ty, sw - u.s(4), thh };
            const bool hov = u.Hit(thumb);
            if (w->barDrag) { if (!u.down) w->barDrag = false;
                else { const float32 t = (u.mp.y - w->barOff - view.y) / (view.h - thh);
                       w->scroll = t * w->scrollMax; if (w->scroll < 0.f) w->scroll = 0.f; if (w->scroll > w->scrollMax) w->scroll = w->scrollMax; } }
            else if (hov && u.click && !blockBg) { w->barDrag = true; w->barOff = u.mp.y - ty; }
            u.dl->AddRectFilled(thumb, (w->barDrag || hov) ? NkColor{ 96,104,114,255 } : NkColor{ 56,63,72,255 }, (sw - u.s(4)) * 0.5f);
        }
        // Scrollbar horizontale (si le contenu depasse en largeur).
        if (w->hscrollMax > 0.5f) {
            const float32 sh = u.s(10);
            const float32 availW = view.w - (w->scrollMax > 0.5f ? u.s(14) : 0.f);
            const NkRect track = { view.x, view.y + view.h - sh, availW, sh };
            u.dl->AddRectFilled(track, NkColor{ 18,21,26,160 }, sh * 0.5f);
            float32 thw = availW * (view.w / bodyW); if (thw < u.s(28)) thw = u.s(28);
            const float32 tx = track.x + (availW - thw) * (w->hscroll / w->hscrollMax);
            const NkRect thumb = { tx, track.y + u.s(2), thw, sh - u.s(4) };
            const bool hov = u.Hit(thumb);
            if (w->hbarDrag) { if (!u.down) w->hbarDrag = false;
                else { const float32 t = (u.mp.x - w->hbarOff - track.x) / (availW - thw);
                       w->hscroll = t * w->hscrollMax; if (w->hscroll < 0.f) w->hscroll = 0.f; if (w->hscroll > w->hscrollMax) w->hscroll = w->hscrollMax; } }
            else if (hov && u.click && !blockBg) { w->hbarDrag = true; w->hbarOff = u.mp.x - tx; }
            u.dl->AddRectFilled(thumb, (w->hbarDrag || hov) ? NkColor{ 96,104,114,255 } : NkColor{ 56,63,72,255 }, (sh - u.s(4)) * 0.5f);
        }

        // ── Pied (Suivant grise si etape invalide) ──
        int32 result = 0;
        const bool nextOk = (w->step != 0) || w->Step1Valid();
        const int32 fa = NkWizFooter(u, { r.x, r.y + r.h - footH, r.w, footH }, w->step, 4, blockBg, nextOk, NkT("nws.create.ws"), true, ic.valideSimple);
        if (fa == 1) result = 1;                                  // Annuler -> Accueil
        else if (fa == -1 && w->step > 0) { w->step--; w->focus = -1; w->scroll = 0.f; }
        else if (fa == 2 && nextOk) { if (w->step < 4) { w->step++; w->focus = -1; w->scroll = 0.f; } else { if (w->Generate(dlg)) result = 1; } }
        else if (fa == 3 && nextOk) { if (w->Generate(dlg)) result = 1; }   // Creer maintenant (defauts)

        // ── Modale « Ajouter / Editer un projet » (3 etapes, par-dessus la page) ──
        if (w->projDlg) NkWizProjDialog(u, r, w, dlg, dt, (w->comboOpen >= 0), ic);
        // ── Modale « Ajouter un Toolchain Manuel » (etape 3) ──
        if (w->tcDlg) NkWizTcDialog(u, r, w, dlg, dt, (w->comboOpen >= 0), ic);

        // ── Dropdown de combo (rendu PAR-DESSUS tout) ──
        if (w->comboOpen >= 0 && w->comboOpts && w->comboSel) {
            const float32 ih = u.s(26);
            // Largeur = au moins celle du combo, sinon celle de l'option la plus longue (le menu ne tronque plus).
            float32 ddw = w->comboR.w;
            for (int32 k = 0; k < w->comboN; ++k) { const float32 tw = u.TextW(w->comboOpts[k]) + u.s(30); if (tw > ddw) ddw = tw; }
            float32 ddx = w->comboR.x;
            if (ddx + ddw > r.x + r.w - u.s(8)) ddx = r.x + r.w - u.s(8) - ddw;   // garde le menu dans la fenetre
            if (ddx < r.x + u.s(8)) ddx = r.x + u.s(8);
            float32 ddy = w->comboR.y + w->comboR.h + u.s(2); const float32 ddh = w->comboN * ih + u.s(6);
            if (ddy + ddh > r.y + r.h - u.s(8)) ddy = w->comboR.y - ddh - u.s(2);  // bascule au-dessus si deborde en bas
            const NkRect dd = { ddx, ddy, ddw, ddh };
            u.dl->AddRectFilled({ dd.x + u.s(2), dd.y + u.s(3), dd.w, dd.h }, NkColor{ 0,0,0,90 }, NkR::md * u.S);
            u.Panel(dd, NkCol::surface, NkCol::primary, NkR::md * u.S);
            bool chose = false;
            for (int32 k = 0; k < w->comboN; ++k) {
                const NkRect ir = { dd.x + u.s(4), dd.y + u.s(3) + k * ih, dd.w - u.s(8), ih };
                const bool hv = u.Hit(ir);
                if (hv || k == *w->comboSel) u.Rect(ir, NkCol::hover, NkR::sm * u.S);
                u.TextEllipsis(ir.x + u.s(8), ir.y + (ih - u.Lh()) * 0.5f, ir.w - u.s(12), w->comboOpts[k], NkCol::foreground);
                if (hv && u.click) { *w->comboSel = k; chose = true; }
            }
            // Ferme des qu'on clique HORS de la liste (sauf la frame d'ouverture).
            if (chose || (u.click && !u.Hit(dd) && !w->comboJustOpened)) w->comboOpen = -1;
            w->comboJustOpened = false;
        }

        // Clic dans le vide (hors champ, hors dialogues) -> le champ actif perd le focus.
        if (u.click && !blockBg && !w->focusClaimed && u.Hit(r)) w->focus = -1;
        // Echap -> ferme combo, sinon dialog projet, sinon defocus, sinon annuler.
        if (u.ctx->input.KeyPressed(NkGuiKey::Escape)) {
            if (w->comboOpen >= 0) w->comboOpen = -1;
            else if (w->projDlg) { w->projDlg = false; w->focus = -1; }
            else if (w->focus >= 0) w->focus = -1;
            else result = 1;
        }
        return result;
    }

} // namespace nkcode
} // namespace nkentseu
