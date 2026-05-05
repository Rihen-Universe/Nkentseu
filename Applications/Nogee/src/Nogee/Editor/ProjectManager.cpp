#include "ProjectManager.h"
#include "NKSerialization/NkSerializer.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKFileSystem/NkPath.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace Unkeny {

        void ProjectManager::NewProject(const char* name, const char* rootDir) noexcept {
            mConfig              = NkProjectConfig{};
            mConfig.name         = NkString(name);
            mProjectDir          = NkString(rootDir);
            mAssetsDir           = mProjectDir + "/" + mConfig.assetsDir;
            mProjectPath         = mProjectDir + "/" + NkString(name) + ".nkproj";
            mOpen                = true;
            mModified            = true;
            logger.Infof("[ProjectManager] Nouveau projet: {}\n", name);
        }

        bool ProjectManager::Save(const char* path) noexcept {
            const char* savePath = path ? path : mProjectPath.CStr();
            if (!savePath || !savePath[0]) {
                logger.Errorf("[ProjectManager] Chemin de sauvegarde vide\n");
                return false;
            }

            NkArchive archive;
            archive.SetString("name",           mConfig.name.CStr());
            archive.SetString("version",         mConfig.version.CStr());
            archive.SetString("assetsDir",       mConfig.assetsDir.CStr());
            archive.SetString("buildsDir",       mConfig.buildsDir.CStr());
            archive.SetString("startupScene",    mConfig.startupScene.CStr());
            archive.SetString("defaultBackend",  mConfig.defaultBackend.CStr());

            // Scènes
            for (nk_uint32 i = 0; i < mConfig.scenes.Size(); ++i) {
                NkString key = NkFormat("scene_{}", i);
                archive.SetString(key.CStr(), mConfig.scenes[i].CStr());
            }
            archive.SetInt("sceneCount", (nk_int64)mConfig.scenes.Size());

            NkJSONWriter writer;
            bool ok = writer.Write(archive, savePath);
            if (ok) {
                mProjectPath  = NkString(savePath);
                mModified     = false;
                logger.Infof("[ProjectManager] Projet sauvegardé: {}\n", savePath);
            } else {
                logger.Errorf("[ProjectManager] Erreur sauvegarde: {}\n", savePath);
            }
            return ok;
        }

        bool ProjectManager::Load(const char* path) noexcept {
            NkJSONReader reader;
            NkArchive archive;
            if (!reader.Read(archive, path)) {
                logger.Errorf("[ProjectManager] Lecture JSON échouée: {}\n", path);
                return false;
            }

            NkString s;
            if (archive.GetString("name",           s)) mConfig.name           = s;
            if (archive.GetString("version",         s)) mConfig.version         = s;
            if (archive.GetString("assetsDir",       s)) mConfig.assetsDir       = s;
            if (archive.GetString("buildsDir",       s)) mConfig.buildsDir       = s;
            if (archive.GetString("startupScene",    s)) mConfig.startupScene    = s;
            if (archive.GetString("defaultBackend",  s)) mConfig.defaultBackend  = s;

            nk_int64 sceneCount = 0;
            archive.GetInt("sceneCount", sceneCount);
            mConfig.scenes.Clear();
            for (nk_int64 i = 0; i < sceneCount; ++i) {
                NkString key = NkFormat("scene_{}", i);
                NkString sc;
                if (archive.GetString(key.CStr(), sc)) mConfig.scenes.PushBack(sc);
            }

            mProjectPath = NkString(path);
            // Extraire le dossier
            mProjectDir  = NkPath::GetDirectory(path);
            mAssetsDir   = mProjectDir + "/" + mConfig.assetsDir;
            mOpen        = true;
            mModified    = false;

            logger.Infof("[ProjectManager] Projet chargé: {} ({})\n",
                         mConfig.name.CStr(), path);
            return true;
        }

        NkString ProjectManager::AbsPath(const char* rel) const noexcept {
            if (!rel) return mProjectDir;
            return mProjectDir + "/" + NkString(rel);
        }

        NkString ProjectManager::RelPath(const char* abs) const noexcept {
            if (!abs) return "";
            NkString s(abs);
            if (s.StartsWith(mProjectDir)) {
                NkString rel = s.Substring(mProjectDir.Length());
                if (!rel.IsEmpty() && (rel[0] == '/' || rel[0] == '\\'))
                    rel = rel.Substring(1);
                return rel;
            }
            return s;
        }

        void ProjectManager::AddScene(const NkString& scenePath) noexcept {
            for (auto& s : mConfig.scenes) if (s == scenePath) return;
            mConfig.scenes.PushBack(scenePath);
            mModified = true;
        }

        void ProjectManager::RemoveScene(const NkString& scenePath) noexcept {
            for (nk_isize i = (nk_isize)mConfig.scenes.Size()-1; i >= 0; --i) {
                if (mConfig.scenes[i] == scenePath) {
                    mConfig.scenes.EraseAt((nk_usize)i);
                    mModified = true;
                    break;
                }
            }
        }

    } // namespace Unkeny
} // namespace nkentseu
