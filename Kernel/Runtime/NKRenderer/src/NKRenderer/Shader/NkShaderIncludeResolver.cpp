// =============================================================================
// NkShaderIncludeResolver.cpp  — M.5 Material Functions
// =============================================================================
#include "NkShaderIncludeResolver.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include <cstdio>

namespace nkentseu {
    namespace renderer {

        // Profondeur max de récursion (anti-runaway pour chaînes pathologiques).
        static constexpr int kMaxIncludeDepth = 16;

        // Helper : isole le quote/bracket de la directive et retourne l'argument.
        // Ex: '#include "foo.glsli"'  → "foo.glsli"
        //     '#include <foo.glsli>'  → "foo.glsli"
        // Retourne string vide si malformé.
        static NkString ParseIncludeArg(const NkString& line) {
            // Trouve "include"
            const char* s = line.CStr();
            const uint32 n = (uint32)line.Size();
            uint32 i = 0;
            // Skip whitespace de début
            while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
            if (i >= n || s[i] != '#') return "";
            ++i;
            while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
            // Doit commencer par "include"
            const char* kKw = "include";
            for (uint32 k = 0; k < 7; ++k) {
                if (i + k >= n || s[i + k] != kKw[k]) return "";
            }
            i += 7;
            // Skip whitespace
            while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
            // Quote ouvrant
            char openQ = (i < n) ? s[i] : '\0';
            char closeQ = '\0';
            if (openQ == '"')      closeQ = '"';
            else if (openQ == '<') closeQ = '>';
            else                    return "";
            ++i;
            // Lit jusqu'au quote fermant
            uint32 start = i;
            while (i < n && s[i] != closeQ) ++i;
            if (i >= n) return "";   // pas de quote fermant
            NkString arg;
            arg.Resize(i - start);
            for (uint32 k = 0; k < i - start; ++k) arg[k] = s[start + k];
            return arg;
        }

        // Helper : retourne le dossier parent d'un path (ex: "a/b/c.glsl" -> "a/b/").
        // Si pas de séparateur, retourne string vide.
        static NkString DirOf(const NkString& path) {
            const uint32 n = (uint32)path.Size();
            for (int32 i = (int32)n - 1; i >= 0; --i) {
                if (path[(uint32)i] == '/' || path[(uint32)i] == '\\') {
                    NkString d;
                    d.Resize((uint32)i + 1u);
                    for (uint32 k = 0; k <= (uint32)i; ++k) d[k] = path[k];
                    return d;
                }
            }
            return "";
        }

        // Helper : vérifie si un chemin est déjà dans la liste visitée.
        static bool AlreadyVisited(const NkVector<NkString>& visited, const NkString& path) {
            for (auto& v : visited) {
                if (v == path) return true;
            }
            return false;
        }

        // ── Public API ────────────────────────────────────────────────────────
        NkString NkShaderIncludeResolver::Resolve(const NkString& source,
                                                    const NkString& currentFilePath) {
            NkVector<NkString> visited;
            return ResolveRecursive(source, currentFilePath, visited, 0);
        }

        // ── Path resolution ───────────────────────────────────────────────────
        NkString NkShaderIncludeResolver::ResolveIncludePath(const NkString& includeArg,
                                                              const NkString& currentFilePath) {
            // 1. Chemin absolu commençant par "Resources/" : utilisé tel quel.
            if (includeArg.Size() >= 10) {
                const char* s = includeArg.CStr();
                if (s[0]=='R' && s[1]=='e' && s[2]=='s' && s[3]=='o' && s[4]=='u' &&
                    s[5]=='r' && s[6]=='c' && s[7]=='e' && s[8]=='s' && s[9]=='/') {
                    if (NkFile::Exists(includeArg.CStr())) return includeArg;
                }
            }

            // 2. Chemin commençant par "Include/" : relatif à
            //    "Resources/NKRenderer/Shaders/".
            if (includeArg.Size() >= 8) {
                const char* s = includeArg.CStr();
                if (s[0]=='I' && s[1]=='n' && s[2]=='c' && s[3]=='l' && s[4]=='u' &&
                    s[5]=='d' && s[6]=='e' && s[7]=='/') {
                    NkString p = NkString("Resources/NKRenderer/Shaders/") + includeArg;
                    if (NkFile::Exists(p.CStr())) return p;
                }
            }

            // 3. Relatif au fichier courant : <dirOf(currentFile)>/<includeArg>
            if (!currentFilePath.Empty()) {
                NkString p = DirOf(currentFilePath) + includeArg;
                if (NkFile::Exists(p.CStr())) return p;
            }

            // 4. Fallback : "Resources/NKRenderer/Shaders/Include/<arg>"
            {
                NkString p = NkString("Resources/NKRenderer/Shaders/Include/") + includeArg;
                if (NkFile::Exists(p.CStr())) return p;
            }

            return "";   // introuvable
        }

        // ── Read raw (sans strip d'annotations) ───────────────────────────────
        NkString NkShaderIncludeResolver::ReadRaw(const NkString& path) {
            FILE* f = fopen(path.CStr(), "rb");
            if (!f) return "";
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            NkString raw; raw.Resize((uint32)sz);
            fread(raw.Data(), 1, (size_t)sz, f);
            fclose(f);
            return raw;
        }

        // ── Récursion principale ──────────────────────────────────────────────
        NkString NkShaderIncludeResolver::ResolveRecursive(const NkString& source,
                                                            const NkString& currentFilePath,
                                                            NkVector<NkString>& visited,
                                                            int depth) {
            if (depth > kMaxIncludeDepth) {
                logger.Warnf("[NkShaderIncludeResolver] depth > %d, abandon\n",
                             kMaxIncludeDepth);
                return source;
            }

            // Scan ligne par ligne. Pour chaque ligne commencant par "#include",
            // substitue le contenu du fichier référencé.
            NkString out;
            const char* s = source.CStr();
            const uint32 n = (uint32)source.Size();
            uint32 lineStart = 0;

            for (uint32 i = 0; i <= n; ++i) {
                const bool eol = (i == n) || s[i] == '\n';
                if (!eol) continue;

                // Ligne brute (sans le '\n' final)
                NkString line;
                const uint32 len = i - lineStart;
                line.Resize(len);
                for (uint32 k = 0; k < len; ++k) line[k] = s[lineStart + k];

                // Détecte directive #include
                NkString arg = ParseIncludeArg(line);
                if (!arg.Empty()) {
                    NkString resolved = ResolveIncludePath(arg, currentFilePath);
                    if (resolved.Empty()) {
                        out += "// [NkShaderInclude] FAILED to resolve: ";
                        out += arg;
                        out += "\n";
                        logger.Warnf("[NkShaderIncludeResolver] include introuvable : '%s' (depuis '%s')\n",
                                     arg.CStr(),
                                     currentFilePath.Empty() ? "<inline>" : currentFilePath.CStr());
                    } else if (AlreadyVisited(visited, resolved)) {
                        // Anti-cycle : déjà inclus, skip silencieusement.
                        out += "// [NkShaderInclude] skipped (already included): ";
                        out += resolved;
                        out += "\n";
                    } else {
                        visited.PushBack(resolved);
                        NkString rawSubFile = ReadRaw(resolved);
                        NkString resolvedSub = ResolveRecursive(rawSubFile, resolved,
                                                                  visited, depth + 1);
                        out += "// [NkShaderInclude] begin: ";
                        out += resolved;
                        out += "\n";
                        out += resolvedSub;
                        if (!resolvedSub.Empty() &&
                            resolvedSub[resolvedSub.Size() - 1] != '\n') {
                            out += "\n";
                        }
                        out += "// [NkShaderInclude] end: ";
                        out += resolved;
                        out += "\n";
                    }
                } else {
                    out += line;
                    out += "\n";
                }

                lineStart = i + 1;
            }

            return out;
        }

    } // namespace renderer
} // namespace nkentseu
