// =============================================================================
// FICHIER  : Modules/System/NKFileSystem/src/NKFileSystem/NkFileWatcher.h
// MODULE   : NKFileSystem
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Surveillance des changements de fichiers/repertoires via APIs natives.
//   Linux : inotify, Windows : ReadDirectoryChangesW, Web : fallback sans noyau.
// =============================================================================

#pragma once

#ifndef NK_FILESYSTEM_NKFILEWATCHER_H_INCLUDED
#define NK_FILESYSTEM_NKFILEWATCHER_H_INCLUDED

// -- Dependances internes -----------------------------------------------------
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {

    // =========================================================================
    // Enum : NkFileChangeType
    // =========================================================================
    enum class NkFileChangeType {
        NK_CREATED,
        NK_DELETED,
        NK_MODIFIED,
        NK_RENAMED,
        NK_ATTRIBUTE_CHANGED
    };

    // =========================================================================
    // Structure : NkFileChangeEvent
    // =========================================================================
    struct NkFileChangeEvent {
        NkFileChangeType Type;
        NkString Path;
        NkString OldPath;
        nk_int64 Timestamp;

        NkFileChangeEvent();
        NkFileChangeEvent(NkFileChangeType type, const char* path);
    };

    // =========================================================================
    // Interface : NkFileWatcherCallback
    // =========================================================================
    class NkFileWatcherCallback {
        public:
            virtual ~NkFileWatcherCallback() {}
            virtual void OnFileChanged(const NkFileChangeEvent& event) = 0;
    };

    // =========================================================================
    // Classe : NkFileWatcher
    // =========================================================================
    class NkFileWatcher {

        // -- Section privee ----------------------------------------------------
        private:

            // Handle natif eventuel (non utilise sur toutes les plateformes).
            void* mHandle;

            // Repertoire surveille.
            NkString mPath;

            // Callback utilisateur.
            NkFileWatcherCallback* mCallback;

            // Etat runtime.
            bool mIsWatching;
            bool mRecursive;

            // Handle thread plateforme.
            void* mThread;

            void WatchThread();
            static void* ThreadProc(void* param);

        // -- Section publique --------------------------------------------------
        public:

            // -----------------------------------------------------------------
            // Constructeurs / destructeur
            // -----------------------------------------------------------------
            NkFileWatcher();
            NkFileWatcher(const char* path, NkFileWatcherCallback* callback, bool recursive = false);
            NkFileWatcher(const NkPath& path, NkFileWatcherCallback* callback, bool recursive = false);
            ~NkFileWatcher();

            // -----------------------------------------------------------------
            // Copie interdite
            // -----------------------------------------------------------------
            NkFileWatcher(const NkFileWatcher&) = delete;
            NkFileWatcher& operator=(const NkFileWatcher&) = delete;

            // -----------------------------------------------------------------
            // Controle
            // -----------------------------------------------------------------
            bool Start();
            void Stop();
            bool IsWatching() const;

            // -----------------------------------------------------------------
            // Configuration
            // -----------------------------------------------------------------
            void SetPath(const char* path);
            void SetPath(const NkPath& path);
            void SetCallback(NkFileWatcherCallback* callback);
            void SetRecursive(bool recursive);

            const NkString& GetPath() const;
            bool IsRecursive() const;
    };

#if defined(NK_CPP11)

    // =========================================================================
    // Classe : NkSimpleFileWatcher
    // =========================================================================
    // Adaptateur pratique pour callback fonctionnel C-style.
    class NkSimpleFileWatcher : public NkFileWatcherCallback {
        private:
            NkFileWatcher mWatcher;

        public:
            using CallbackFunc = void(*)(const NkFileChangeEvent&);
            CallbackFunc OnChanged;

            NkSimpleFileWatcher(const char* path, bool recursive = false)
                : mWatcher(path, this, recursive)
                , OnChanged(nullptr) {
            }

            void OnFileChanged(const NkFileChangeEvent& event) override {
                if (OnChanged) {
                    OnChanged(event);
                }
            }

            bool Start() {
                return mWatcher.Start();
            }

            void Stop() {
                mWatcher.Stop();
            }

            bool IsWatching() const {
                return mWatcher.IsWatching();
            }
    };

#endif // defined(NK_CPP11)

    // -------------------------------------------------------------------------
    // Compatibilite legacy : nkentseu::entseu::*
    // -------------------------------------------------------------------------
    namespace entseu {
        using NkFileChangeType = ::nkentseu::NkFileChangeType;
        using NkFileChangeEvent = ::nkentseu::NkFileChangeEvent;
        using NkFileWatcherCallback = ::nkentseu::NkFileWatcherCallback;
        using NkFileWatcher = ::nkentseu::NkFileWatcher;
#if defined(NK_CPP11)
        using NkSimpleFileWatcher = ::nkentseu::NkSimpleFileWatcher;
#endif
    } // namespace entseu

} // namespace nkentseu

#endif // NK_FILESYSTEM_NKFILEWATCHER_H_INCLUDED

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

// =============================================================================
// EXEMPLES D'UTILISATION - NkFileWatcher
// =============================================================================
//
// -- Callback OO --------------------------------------------------------------
//
//   class MyWatcherCb : public NkFileWatcherCallback {
//       public:
//           void OnFileChanged(const NkFileChangeEvent& e) override {
//               // e.Type, e.Path, e.Timestamp
//           }
//   };
//
//   MyWatcherCb cb;
//   NkFileWatcher watcher("assets", &cb, true);
//   watcher.Start();
//   watcher.Stop();
//
// -- Callback simple (NK_CPP11) ----------------------------------------------
//
//   NkSimpleFileWatcher watcher("assets", true);
//   watcher.OnChanged = [](const NkFileChangeEvent& e) {
//       // traitement evenement
//   };
//   watcher.Start();
//
// =============================================================================
