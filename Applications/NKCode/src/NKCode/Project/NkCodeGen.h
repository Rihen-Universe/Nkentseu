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
