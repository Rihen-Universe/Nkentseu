#pragma once
// =============================================================================
// NkCodeGen.h — Generation de fichiers .jenga (workspace + projet) par NKCode.
//
// Permet a l'utilisateur de creer un workspace ou un projet via l'UI, sans
// editer le .jenga a la main (mais il reste libre de le faire). Les fichiers
// generes suivent le DSL Jenga ; un include() est ajoute au workspace courant.
// =============================================================================

#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace nkcode {

        // Genre de projet selectionnable a la creation (ordre = combo UI).
        struct NkKindDef { const char* label; const char* dslFn; bool app; };
        inline const NkKindDef* NkKinds(int32* n) noexcept {
            static const NkKindDef k[] = {
                { "Application console", "consoleapp",  true  },
                { "Application fenetree", "windowedapp", true  },
                { "Bibliotheque statique", "staticlib",  false },
                { "Bibliotheque partagee", "sharedlib",  false },
            };
            if (n) *n = 4;
            return k;
        }
        struct NkLangDef { const char* label; const char* dsl; const char* dialect; const char* ext; };
        inline const NkLangDef* NkLangs(int32* n) noexcept {
            static const NkLangDef l[] = {
                { "C++",         "C++", "C++17", "cpp" },
                { "C",           "C",   "",      "c"   },
                { "Objective-C", "ObjC","",      "m"   },
            };
            if (n) *n = 3;
            return l;
        }

        // ── helpers de nom : garde [A-Za-z0-9_], remplace le reste par '_' ──
        inline NkString NkSanitizeName(const char* s) noexcept {
            NkString out;
            for (const char* p = s; p && *p; ++p) {
                const char c = *p;
                const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                             || (c >= '0' && c <= '9') || c == '_';
                out += ok ? c : '_';
            }
            return out;
        }

        // Genere un nouveau workspace : <root>/<name>.jenga (with workspace(...)).
        // Retourne le chemin du .jenga cree (vide si echec / nom invalide).
        inline NkString GenerateWorkspace(const NkPath& root, const char* rawName) noexcept {
            NkString name = NkSanitizeName(rawName);
            if (name.Empty()) return NkString();
            NkPath file = root / (name + ".jenga").CStr();
            if (NkFile::Exists(file)) return NkString();   // ne pas ecraser

            NkString c;
            c += "#!/usr/bin/env python3\n";
            c += "# -*- coding: utf-8 -*-\n";
            c += "\"\"\""; c += name; c += " - workspace genere par NKCode.\"\"\"\n\n";
            c += "from Jenga import *\n";
            c += "from jengaconfig import *\n\n\n";
            c += "with workspace(\""; c += name; c += "\", location=\".\"):\n";
            c += "    configurations([\"Debug\", \"Release\"])\n\n";
            c += "    # Les projets sont ajoutes ici via include()\n";
            c += "    # (menu Projet > Nouveau projet dans NKCode).\n";
            if (!NkFile::WriteAllText(file, c)) return NkString();
            return file.ToString();
        }

        inline bool StrEqA(const char* a, const char* b) noexcept {
            if (!a || !b) return false; while (*a && *b) { if (*a != *b) return false; ++a; ++b; } return *a == *b;
        }

        // Toolchain par defaut + variable d'environnement de detection, par OS.
        // env vide => pas de detection par env (toolchain suppose present).
        struct NkToolchainInfo { const char* os; const char* toolchain; const char* env; };
        inline const NkToolchainInfo* NkToolchains(int32* n) noexcept {
            static const NkToolchainInfo t[] = {
                { "Windows",    "clang / msvc",  ""                 },
                { "Linux",      "clang-native",  ""                 },
                { "macOS",      "apple-clang",   ""                 },
                { "Android",    "android-ndk",   "ANDROID_NDK_HOME" },
                { "iOS",        "apple-clang",   ""                 },
                { "Web",        "emscripten",    "EMSDK"            },
                { "HarmonyOS",  "ohos-ndk",      "OHOS_NDK_HOME"    },
                { "XboxSeries", "msvc (GDK)",    "GameDK"           },
            };
            if (n) *n = 8; return t;
        }

        // Tables de reference (alignees sur le DSL Jenga).
        inline const char* const* NkConfigNames(int32* n) noexcept {
            static const char* c[] = { "Debug", "Release", "Profile", "Shipping" }; if (n) *n = 4; return c;
        }
        inline const char* const* NkArchNames(int32* n) noexcept {
            static const char* a[] = { "x86_64", "x86", "arm64", "arm", "wasm32" }; if (n) *n = 5; return a;
        }

        // Toutes les proprietes de creation d'un workspace (cf. DSL Jenga).
        struct NkWorkspaceOpts {
            const char*        name        = "";
            bool               cfg[4]      = { true, true, false, false };   // Debug, Release, Profile, Shipping
            const bool*        os          = nullptr;                        // flags, taille nOs
            const char* const* osNames     = nullptr;
            int32              nOs         = 0;
            bool               arch[5]     = {};                             // x86_64, x86, arm64, arm, wasm32
            const char*        startProject= "";
            const char*        toolchain   = "";
            bool               dutc        = false;
            bool               dute        = false;
            const char*        androidSdk  = "";
            const char*        androidNdk  = "";
            const char*        javaJdk     = "";
            const char*        harmonySdk  = "";
            const char*        gdkPath     = "";
        };

        // Genere un workspace COMPLET dans `dir` a partir des options (DSL Jenga :
        // configurations / targetoses / targetarchs / usetoolchain / startproject /
        // dutc / dute / *sdkpath). Retourne le chemin du .jenga (vide si echec).
        inline NkString GenerateWorkspaceEx(const NkPath& dir, const NkWorkspaceOpts& o) noexcept {
            NkString name = NkSanitizeName(o.name);
            if (name.Empty()) return NkString();
            if (!NkDirectory::Exists(dir) && !NkDirectory::CreateRecursive(dir)) return NkString();
            NkPath file = dir / (name + ".jenga").CStr();
            if (NkFile::Exists(file)) return NkString();   // ne pas ecraser

            auto join = [](const bool* flags, const char* const* names, int32 n) -> NkString {
                NkString s; for (int32 i = 0; i < n; ++i) if (flags[i]) { if (!s.Empty()) s += ", "; s += "\""; s += names[i]; s += "\""; } return s;
            };
            int32 nC = 0, nA = 0; const char* const* cfgN = NkConfigNames(&nC); const char* const* archN = NkArchNames(&nA);
            NkString cfgs = join(o.cfg, cfgN, nC); if (cfgs.Empty()) cfgs = "\"Debug\", \"Release\"";
            NkString oss  = o.os ? join(o.os, o.osNames, o.nOs) : NkString();
            NkString arcs = join(o.arch, archN, nA);

            // un OS donne est-il selectionne ? (pour les chemins SDK conditionnels)
            auto osOn = [&](const char* nm) -> bool {
                if (!o.os) return false;
                for (int32 i = 0; i < o.nOs; ++i) if (o.os[i] && StrEqA(o.osNames[i], nm)) return true;
                return false;
            };

            NkString c;
            c += "#!/usr/bin/env python3\n# -*- coding: utf-8 -*-\n";
            c += "\"\"\""; c += name; c += " - workspace genere par NKCode.\"\"\"\n\n";
            c += "from Jenga import *\nfrom jengaconfig import *\n\n\n";
            c += "with workspace(\""; c += name; c += "\", location=\".\"):\n";
            c += "    configurations(["; c += cfgs; c += "])\n";
            if (!oss.Empty())  { c += "    targetoses(["; c += oss; c += "])\n"; }
            if (!arcs.Empty()) { c += "    targetarchs(["; c += arcs; c += "])\n"; }
            if (o.toolchain && *o.toolchain)       { c += "    usetoolchain(\""; c += o.toolchain; c += "\")\n"; }
            if (o.startProject && *o.startProject) { c += "    startproject(\""; c += o.startProject; c += "\")\n"; }
            if (o.dutc) c += "    dutc(True)\n";
            if (o.dute) c += "    dute(True)\n";
            // Chemins SDK (uniquement si l'OS correspondant est cible ET le chemin fourni)
            if (osOn("Android")) {
                if (o.androidSdk && *o.androidSdk) { c += "    androidsdkpath(r\""; c += o.androidSdk; c += "\")\n"; }
                if (o.androidNdk && *o.androidNdk) { c += "    androidndkpath(r\""; c += o.androidNdk; c += "\")\n"; }
                if (o.javaJdk    && *o.javaJdk)    { c += "    javajdkpath(r\"";    c += o.javaJdk;    c += "\")\n"; }
            }
            if (osOn("HarmonyOS") && o.harmonySdk && *o.harmonySdk) { c += "    harmonysdk(r\""; c += o.harmonySdk; c += "\")\n"; }
            if ((osOn("XboxSeries") || osOn("XboxOne")) && o.gdkPath && *o.gdkPath) { c += "    gdkpath(r\""; c += o.gdkPath; c += "\")\n"; }
            c += "\n    # Les projets sont ajoutes ici via include() (menu Nouveau projet).\n";
            if (!NkFile::WriteAllText(file, c)) return NkString();
            return file.ToString();
        }

        // Genere un nouveau projet dans <root>/<name>/ :
        //   - <name>/<name>.jenga (with project(...))
        //   - <name>/src/main.<ext> (stub minimal selon le genre)
        // puis ajoute `with include("<name>/<name>.jenga"): pass` au workspace.
        // Retourne le chemin du .jenga projet (vide si echec).
        inline NkString GenerateProject(const NkPath& root, const NkPath& workspaceJenga,
                                        const char* rawName, int32 kindIdx, int32 langIdx) noexcept {
            NkString name = NkSanitizeName(rawName);
            if (name.Empty()) return NkString();
            int32 nk = 0, nl = 0; const NkKindDef* K = NkKinds(&nk); const NkLangDef* L = NkLangs(&nl);
            if (kindIdx < 0 || kindIdx >= nk) kindIdx = 0;
            if (langIdx < 0 || langIdx >= nl) langIdx = 0;
            const NkKindDef& kd = K[kindIdx];
            const NkLangDef& ld = L[langIdx];

            NkPath dir = root / name.CStr();
            if (NkDirectory::Exists(dir)) return NkString();   // ne pas ecraser un dossier existant
            NkPath srcDir = dir / "src";
            if (!NkDirectory::CreateRecursive(srcDir)) return NkString();

            // ── source stub ──
            NkString mainPath = NkString(name) + "/src/main." + ld.ext;
            NkPath srcFile = srcDir / (NkString("main.") + ld.ext).CStr();
            NkString src;
            const bool isC = (NkString(ld.dsl) == "C");
            if (kd.app) {
                src += isC ? "#include <stdio.h>\n\n" : "#include <cstdio>\n\n";
                src += "int main(int argc, char** argv) {\n";
                src += "    (void)argc; (void)argv;\n";
                src += "    printf(\"";  src += name; src += " - genere par NKCode\\n\");\n";
                src += "    return 0;\n}\n";
            } else {
                src += "// "; src += name; src += " - bibliotheque generee par NKCode.\n\n";
                src += isC ? "int " : "int ";
                src += name; src += "_Version(void) { return 1; }\n";
            }
            NkFile::WriteAllText(srcFile, src);

            // ── projet .jenga ──
            NkString c;
            c += "#!/usr/bin/env python3\n";
            c += "# -*- coding: utf-8 -*-\n";
            c += "\"\"\""; c += name; c += " - projet genere par NKCode.\"\"\"\n\n";
            c += "from Jenga import *\n";
            c += "from jengaconfig import *\n\n\n";
            c += "with project(\""; c += name; c += "\"):\n";
            c += "    "; c += kd.dslFn; c += "()\n";
            c += "    language(\""; c += ld.dsl; c += "\")\n";
            if (ld.dialect && ld.dialect[0]) { c += "    cppdialect(\""; c += ld.dialect; c += "\")\n"; }
            c += "    location(\".\")\n\n";
            c += "    files([\"src/**."; c += ld.ext; c += "\", \"src/**.h\"])\n\n";
            c += "    objdir(\"%{wks.location}/Build/Obj/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}\")\n";
            c += "    targetdir(\"%{wks.location}/Build/Bin/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}\")\n\n";
            c += "    with filter(\"config:Debug\"):\n";
            c += "        defines([\"_DEBUG\", \"DEBUG\"])\n";
            c += "        optimize(\"Off\")\n";
            c += "        symbols(True)\n\n";
            c += "    with filter(\"config:Release\"):\n";
            c += "        defines([\"NDEBUG\"])\n";
            c += "        optimize(\"Speed\")\n";
            c += "        symbols(False)\n";
            NkPath projJenga = dir / (name + ".jenga").CStr();
            if (!NkFile::WriteAllText(projJenga, c)) return NkString();

            // ── enregistrer dans le workspace via include() ──
            if (NkFile::Exists(workspaceJenga)) {
                NkString ws = NkFile::ReadAllText(workspaceJenga);
                NkString inc;
                inc += "\n    # >>> NKCode: projet "; inc += name; inc += "\n";
                inc += "    with include(\""; inc += name; inc += "/"; inc += name; inc += ".jenga\"):\n";
                inc += "        pass\n";
                ws += inc;
                NkFile::WriteAllText(workspaceJenga, ws);
            }
            return projJenga.ToString();
        }

    } // namespace nkcode
} // namespace nkentseu
