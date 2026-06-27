#pragma once
// =============================================================================
// NkFontPrefs.h — Reglages de polices de l'IDE (interface + code), modifiables
//   par l'utilisateur et PERSISTES dans un petit fichier de config.
//   - Resolution par NOM : police EMBARQUEE (Inter, DejaVu, Karla...) ou, sous
//     Windows, police SYSTEME chargee a l'execution depuis C:\Windows\Fonts
//     (Consolas, Segoe UI, Courier New... -> aucune redistribution, c'est la
//     licence de l'utilisateur). Repli sur le defaut si introuvable.
//   - Defaut : interface = Inter, code = DejaVu Sans Mono.
// =============================================================================
#include "NKGui/Core/NkGuiFont.h"
#include "NKContainers/String/NkString.h"
#include "NKFont/Embedded/NkFontEmbedded.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
    namespace editorkit {

        struct NkFontPrefs {
            NkString uiFont   = "Inter";
            NkString codeFont = "DejaVuSansMono";
            float32  uiSize   = 16.f;
            float32  codeSize = 15.f;
        };

        // Listes proposees dans l'UI (le resolveur tolere d'autres noms).
        inline const char* const* NkUiFontNames(int32* n)   { static const char* a[] = { "Inter", "Karla", "DroidSans", "Roboto", "Segoe UI", "Arial", "Tahoma", "Verdana" }; *n = 8; return a; }
        inline const char* const* NkCodeFontNames(int32* n) { static const char* a[] = { "DejaVuSansMono", "Cousine", "SourceCodePro", "ProggyClean", "Consolas", "Courier New", "Cascadia Code", "Cascadia Mono", "Lucida Console" }; *n = 9; return a; }

        // Nom -> police embarquee (true si trouvee).
        inline bool NkEmbeddedIdFromName(const NkString& name, NkEmbeddedFontId& out) {
            struct E { const char* n; NkEmbeddedFontId id; };
            static const E t[] = {
                { "Inter", NkEmbeddedFontId::Inter }, { "Karla", NkEmbeddedFontId::Karla },
                { "DroidSans", NkEmbeddedFontId::DroidSans }, { "Roboto", NkEmbeddedFontId::Roboto },
                { "DroidSerif", NkEmbeddedFontId::DroidSerif }, { "Cousine", NkEmbeddedFontId::Cousine },
                { "SourceCodePro", NkEmbeddedFontId::SourceCodePro }, { "ProggyClean", NkEmbeddedFontId::ProggyClean },
                { "ProggyTiny", NkEmbeddedFontId::ProggyTiny }, { "DejaVuSansMono", NkEmbeddedFontId::DejaVuSansMono },
            };
            for (const E& e : t) if (name == e.n) { out = e.id; return true; }
            return false;
        }

        // Nom de police systeme Windows -> nom de fichier dans C:\Windows\Fonts.
        inline const char* NkSystemFontFile(const NkString& name) {
            struct S { const char* n; const char* file; };
            static const S t[] = {
                { "Consolas", "consola.ttf" }, { "Segoe UI", "segoeui.ttf" }, { "Courier New", "cour.ttf" },
                { "Cascadia Code", "CascadiaCode.ttf" }, { "Cascadia Mono", "CascadiaMono.ttf" },
                { "Lucida Console", "lucon.ttf" }, { "Arial", "arial.ttf" }, { "Tahoma", "tahoma.ttf" },
                { "Verdana", "verdana.ttf" }, { "Times New Roman", "times.ttf" },
            };
            for (const S& s : t) if (name == s.n) return s.file;
            return nullptr;
        }

        // Charge `name` (taille `size`) dans `font`. Embarquee d'abord, sinon police
        // systeme (Windows). Retourne false si rien n'a pu etre charge.
        inline bool NkResolveFont(nkgui::NkGuiFont& font, const NkString& name, float32 size) {
            NkEmbeddedFontId id;
            if (NkEmbeddedIdFromName(name, id)) return font.LoadEmbedded(id, size);
#if defined(_WIN32)
            if (const char* file = NkSystemFontFile(name)) {
                NkString path = NkString("C:\\Windows\\Fonts\\") + file;
                return font.LoadFromFile(path.CStr(), size);
            }
#endif
            return false;
        }

        // ── Persistance (fichier clef=valeur) ──────────────────────────────────
        inline NkString NkFontPrefsPath() {
            const char* home =
#if defined(_WIN32)
                std::getenv("USERPROFILE");
#else
                std::getenv("HOME");
#endif
            if (home && *home) return NkString(home) + "/.nkcode_fonts.cfg";
            return NkString("nkcode_fonts.cfg");   // repli : repertoire courant
        }

        inline void NkLoadFontPrefs(NkFontPrefs& p) {
            FILE* f = std::fopen(NkFontPrefsPath().CStr(), "r");
            if (!f) return;
            char line[512];
            while (std::fgets(line, sizeof(line), f)) {
                char* eq = std::strchr(line, '=');
                if (!eq) continue;
                *eq = 0; char* key = line; char* val = eq + 1;
                usize n = std::strlen(val); while (n > 0 && (val[n-1] == '\n' || val[n-1] == '\r' || val[n-1] == ' ')) val[--n] = 0;
                if      (std::strcmp(key, "ui.font")   == 0) p.uiFont   = val;
                else if (std::strcmp(key, "code.font") == 0) p.codeFont = val;
                else if (std::strcmp(key, "ui.size")   == 0) p.uiSize   = static_cast<float32>(std::atof(val));
                else if (std::strcmp(key, "code.size") == 0) p.codeSize = static_cast<float32>(std::atof(val));
            }
            std::fclose(f);
            if (p.uiSize   < 8.f)  p.uiSize   = 8.f;  if (p.uiSize   > 40.f) p.uiSize   = 40.f;
            if (p.codeSize < 8.f)  p.codeSize = 8.f;  if (p.codeSize > 40.f) p.codeSize = 40.f;
        }

        inline void NkSaveFontPrefs(const NkFontPrefs& p) {
            FILE* f = std::fopen(NkFontPrefsPath().CStr(), "w");
            if (!f) return;
            std::fprintf(f, "ui.font=%s\n",   p.uiFont.CStr());
            std::fprintf(f, "ui.size=%d\n",   static_cast<int>(p.uiSize + 0.5f));
            std::fprintf(f, "code.font=%s\n", p.codeFont.CStr());
            std::fprintf(f, "code.size=%d\n", static_cast<int>(p.codeSize + 0.5f));
            std::fclose(f);
        }

    } // namespace editorkit
} // namespace nkentseu
