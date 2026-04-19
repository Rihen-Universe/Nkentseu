#pragma once
// =============================================================================
// Unkeny/Editor/ProjectManager.h
// =============================================================================
// Gestion du projet Unkeny : .nkproj (JSON), chemins des assets,
// scène d'entrée, paramètres build.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace Unkeny {

        struct NkProjectConfig {
            NkString name            = "NewProject";
            NkString version         = "0.1.0";
            NkString assetsDir       = "Assets";
            NkString buildsDir       = "Build";
            NkString startupScene    = "";     // chemin relatif .nkscene
            NkString defaultBackend  = "opengl";

            // Scènes enregistrées dans le projet
            NkVector<NkString> scenes;

            // Scripts de scènes C++ à compiler (hot-reload Phase 5+)
            NkVector<NkString> scriptModules;
        };

        class ProjectManager {
        public:
            ProjectManager() = default;

            // ── Nouveau projet ────────────────────────────────────────────────
            void NewProject(const char* name, const char* rootDir) noexcept;

            // ── Save / Load ───────────────────────────────────────────────────
            bool Save(const char* path = nullptr) noexcept; // null = mProjectPath
            bool Load(const char* path) noexcept;

            // ── Accès ─────────────────────────────────────────────────────────
            [[nodiscard]] bool IsOpen() const noexcept { return mOpen; }
            [[nodiscard]] const NkString& ProjectPath()  const noexcept { return mProjectPath; }
            [[nodiscard]] const NkString& ProjectDir()   const noexcept { return mProjectDir; }
            [[nodiscard]] const NkString& AssetsDir()    const noexcept { return mAssetsDir; }
            [[nodiscard]] NkProjectConfig& Config() noexcept { return mConfig; }
            [[nodiscard]] const NkProjectConfig& Config() const noexcept { return mConfig; }

            // Chemin absolu depuis un chemin relatif au projet
            NkString AbsPath(const char* relPath) const noexcept;

            // Chemin relatif depuis un chemin absolu
            NkString RelPath(const char* absPath) const noexcept;

            // Enregistre une scène dans la liste du projet
            void AddScene(const NkString& scenePath) noexcept;
            void RemoveScene(const NkString& scenePath) noexcept;

            bool IsModified() const noexcept { return mModified; }
            void MarkModified() noexcept { mModified = true; }
            void ClearModified() noexcept { mModified = false; }

        private:
            NkProjectConfig mConfig;
            NkString        mProjectPath;   // chemin vers le .nkproj
            NkString        mProjectDir;    // dossier racine du projet
            NkString        mAssetsDir;     // chemin absolu vers Assets/
            bool            mOpen     = false;
            bool            mModified = false;
        };

    } // namespace Unkeny
} // namespace nkentseu
