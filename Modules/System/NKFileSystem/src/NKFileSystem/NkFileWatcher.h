// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkFileWatcher.h
// DESCRIPTION: File system watcher for monitoring changes
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILEWATCHER_H_INCLUDED
#define NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILEWATCHER_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {
    namespace entseu {
        
        /**
         * @brief File change types
         */
        enum class NkFileChangeType {
            NK_CREATED,        // File or directory created
            NK_DELETED,        // File or directory deleted
            NK_MODIFIED,       // File content modified
            NK_RENAMED,        // File or directory renamed
            NK_ATTRIBUTE_CHANGED // File attributes changed
        };
        
        /**
         * @brief File change event
         */
        struct NkFileChangeEvent {
            NkFileChangeType Type;
            NkString Path;
            NkString OldPath;  // For rename events
            nk_int64 Timestamp;
            
            NkFileChangeEvent();
            NkFileChangeEvent(NkFileChangeType type, const char* path);
        };
        
        /**
         * @brief File watcher callback
         */
        class NkFileWatcherCallback {
        public:
            virtual ~NkFileWatcherCallback() {}
            virtual void OnFileChanged(const NkFileChangeEvent& event) = 0;
        };
        
        /**
         * @brief File system watcher
         * 
         * Surveille un répertoire pour détecter les changements.
         * Utilise les APIs natives (inotify sur Linux, ReadDirectoryChangesW sur Windows).
         * 
         * @example
         * class MyCallback : public NkFileWatcherCallback {
         *     void OnFileChanged(const NkFileChangeEvent& event) override {
         *         printf("File changed: %s\n", event.Path.CStr());
         *     }
         * };
         * 
         * MyCallback callback;
         * NkFileWatcher watcher("data", &callback);
         * watcher.Start();
         * // ... do work ...
         * watcher.Stop();
         */
        class NkFileWatcher {
        private:
            void* mHandle;  // Platform-specific handle
            NkString mPath;
            NkFileWatcherCallback* mCallback;
            bool mIsWatching;
            bool mRecursive;
            void* mThread;  // Thread handle
            
            void WatchThread();
            static void* ThreadProc(void* param);
            
        public:
            // Constructors
            NkFileWatcher();
            NkFileWatcher(const char* path, NkFileWatcherCallback* callback, bool recursive = false);
            NkFileWatcher(const NkPath& path, NkFileWatcherCallback* callback, bool recursive = false);
            ~NkFileWatcher();
            
            // Non-copyable
            NkFileWatcher(const NkFileWatcher&) = delete;
            NkFileWatcher& operator=(const NkFileWatcher&) = delete;
            
            // Control
            bool Start();
            void Stop();
            bool IsWatching() const;
            
            // Configuration
            void SetPath(const char* path);
            void SetPath(const NkPath& path);
            void SetCallback(NkFileWatcherCallback* callback);
            void SetRecursive(bool recursive);
            
            const NkString& GetPath() const;
            bool IsRecursive() const;
        };
        
        /**
         * @brief Simple file watcher with lambda callback (C++11)
         * 
         * @example
         * NkSimpleFileWatcher watcher("data");
         * watcher.OnChanged = [](const NkFileChangeEvent& event) {
         *     printf("Changed: %s\n", event.Path.CStr());
         * };
         * watcher.Start();
         */
        #if defined(NK_CPP11)
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
            
            bool Start() { return mWatcher.Start(); }
            void Stop() { mWatcher.Stop(); }
            bool IsWatching() const { return mWatcher.IsWatching(); }
        };
        #endif
        
    } // namespace entseu
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILEWATCHER_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
