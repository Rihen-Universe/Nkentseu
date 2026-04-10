// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkFileWatcher.cpp
// DESCRIPTION: File system watcher implementation
// AUTEUR: Rihen
// DATE: 2026-02-10
// -----------------------------------------------------------------------------

#include "NKFileSystem/NkFileWatcher.h"
#include <cstring>
#include <ctime>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#elif defined(__EMSCRIPTEN__)
    // WASM fallback: no native kernel watcher API.
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/inotify.h>
    #include <sys/select.h>
    #include <errno.h>
#endif

namespace nkentseu {
    namespace entseu {
        
        NkFileChangeEvent::NkFileChangeEvent()
            : Type(NkFileChangeType::NK_MODIFIED)
            , Path()
            , OldPath()
            , Timestamp(0) {
        }
        
        NkFileChangeEvent::NkFileChangeEvent(NkFileChangeType type, const char* path)
            : Type(type)
            , Path(path)
            , OldPath()
            , Timestamp(static_cast<nk_int64>(time(nullptr))) {
        }
        
        NkFileWatcher::NkFileWatcher()
            : mHandle(nullptr)
            , mPath()
            , mCallback(nullptr)
            , mIsWatching(false)
            , mRecursive(false)
            , mThread(nullptr) {
        }
        
        NkFileWatcher::NkFileWatcher(const char* path, NkFileWatcherCallback* callback, bool recursive)
            : mHandle(nullptr)
            , mPath(path)
            , mCallback(callback)
            , mIsWatching(false)
            , mRecursive(recursive)
            , mThread(nullptr) {
        }
        
        NkFileWatcher::NkFileWatcher(const NkPath& path, NkFileWatcherCallback* callback, bool recursive)
            : mHandle(nullptr)
            , mPath(path.ToString())
            , mCallback(callback)
            , mIsWatching(false)
            , mRecursive(recursive)
            , mThread(nullptr) {
        }
        
        NkFileWatcher::~NkFileWatcher() {
            Stop();
        }
        
        #ifdef _WIN32
        void* NkFileWatcher::ThreadProc(void* param) {
            NkFileWatcher* watcher = static_cast<NkFileWatcher*>(param);
            watcher->WatchThread();
            return nullptr;
        }
        
        void NkFileWatcher::WatchThread() {
            HANDLE hDir = CreateFileA(
                mPath.CStr(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                NULL
            );
            
            if (hDir == INVALID_HANDLE_VALUE) {
                return;
            }
            
            const DWORD BUFFER_SIZE = 4096;
            char buffer[BUFFER_SIZE];
            DWORD bytesReturned;
            OVERLAPPED overlapped = {0};
            overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            
            while (mIsWatching) {
                BOOL success = ReadDirectoryChangesW(
                    hDir,
                    buffer,
                    BUFFER_SIZE,
                    mRecursive ? TRUE : FALSE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                    FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                    FILE_NOTIFY_CHANGE_LAST_WRITE,
                    &bytesReturned,
                    &overlapped,
                    NULL
                );
                
                if (!success) {
                    break;
                }
                
                DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 1000);
                if (waitResult == WAIT_OBJECT_0) {
                    GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE);
                    
                    if (bytesReturned > 0 && mCallback) {
                        FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
                        
                        while (true) {
                            // Convert wchar to char (simplified)
                            char filename[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, info->FileName, 
                                              info->FileNameLength / sizeof(WCHAR),
                                              filename, MAX_PATH, NULL, NULL);
                            
                            NkFileChangeEvent event;
                            event.Path = (NkPath(mPath.CStr()) / filename).ToString();
                            event.Timestamp = static_cast<nk_int64>(time(nullptr));
                            
                            switch (info->Action) {
                                case FILE_ACTION_ADDED:
                                    event.Type = NkFileChangeType::NK_CREATED;
                                    break;
                                case FILE_ACTION_REMOVED:
                                    event.Type = NkFileChangeType::NK_DELETED;
                                    break;
                                case FILE_ACTION_MODIFIED:
                                    event.Type = NkFileChangeType::NK_MODIFIED;
                                    break;
                                case FILE_ACTION_RENAMED_OLD_NAME:
                                case FILE_ACTION_RENAMED_NEW_NAME:
                                    event.Type = NkFileChangeType::NK_RENAMED;
                                    break;
                            }
                            
                            mCallback->OnFileChanged(event);
                            
                            if (info->NextEntryOffset == 0) break;
                            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                                reinterpret_cast<char*>(info) + info->NextEntryOffset
                            );
                        }
                    }
                    
                    ResetEvent(overlapped.hEvent);
                }
            }
            
            CloseHandle(overlapped.hEvent);
            CloseHandle(hDir);
        }
        
        #elif !defined(__EMSCRIPTEN__)

        void* NkFileWatcher::ThreadProc(void* param) {
            NkFileWatcher* watcher = static_cast<NkFileWatcher*>(param);
            watcher->WatchThread();
            return nullptr;
        }

        void NkFileWatcher::WatchThread() {
            int fd = inotify_init();
            if (fd < 0) {
                return;
            }
            
            uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | 
                           IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB;
            
            int wd = inotify_add_watch(fd, mPath.CStr(), mask);
            if (wd < 0) {
                close(fd);
                return;
            }
            
            const size_t EVENT_SIZE = sizeof(struct inotify_event);
            const size_t BUF_LEN = 1024 * (EVENT_SIZE + 16);
            char buffer[BUF_LEN];
            
            while (mIsWatching) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(fd, &fds);
                
                struct timeval tv;
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                
                int ret = select(fd + 1, &fds, NULL, NULL, &tv);
                
                if (ret < 0) {
                    if (errno == EINTR) continue;
                    break;
                }
                
                if (ret == 0) continue;  // Timeout
                
                ssize_t length = read(fd, buffer, BUF_LEN);
                if (length < 0) {
                    break;
                }
                
                if (mCallback) {
                    size_t i = 0;
                    while (i < static_cast<size_t>(length)) {
                        struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
                        
                        if (event->len > 0) {
                            NkFileChangeEvent changeEvent;
                            changeEvent.Path = (NkPath(mPath.CStr()) / event->name).ToString();
                            changeEvent.Timestamp = static_cast<nk_int64>(time(nullptr));
                            
                            if (event->mask & IN_CREATE) {
                                changeEvent.Type = NkFileChangeType::Created;
                            } else if (event->mask & IN_DELETE) {
                                changeEvent.Type = NkFileChangeType::Deleted;
                            } else if (event->mask & IN_MODIFY) {
                                changeEvent.Type = NkFileChangeType::Modified;
                            } else if (event->mask & (IN_MOVED_FROM | IN_MOVED_TO)) {
                                changeEvent.Type = NkFileChangeType::Renamed;
                            } else if (event->mask & IN_ATTRIB) {
                                changeEvent.Type = NkFileChangeType::AttributeChanged;
                            }
                            
                            mCallback->OnFileChanged(changeEvent);
                        }
                        
                        i += EVENT_SIZE + event->len;
                    }
                }
            }
            
            inotify_rm_watch(fd, wd);
            close(fd);
        }
        
        #endif
        
        bool NkFileWatcher::Start() {
            if (mIsWatching) return true;
            if (mPath.Empty() || !mCallback) return false;
            
            mIsWatching = true;
            
            #ifdef _WIN32
            mThread = reinterpret_cast<void*>(_beginthread(
                reinterpret_cast<void(*)(void*)>(ThreadProc),
                0,
                this
            ));
            #elif defined(__EMSCRIPTEN__)
            // WASM: keep API alive even without kernel notifications.
            // Client code can still query IsWatching()/Stop() deterministically.
            mThread = nullptr;
            return true;
            #else
            pthread_t* thread = new pthread_t;
            if (pthread_create(thread, NULL, ThreadProc, this) == 0) {
                mThread = thread;
            } else {
                delete thread;
                mIsWatching = false;
                return false;
            }
            #endif
            
            return true;
        }
        
        void NkFileWatcher::Stop() {
            if (!mIsWatching) return;
            
            mIsWatching = false;
            
            if (mThread) {
                #ifdef _WIN32
                WaitForSingleObject(mThread, INFINITE);
                CloseHandle(mThread);
                #elif !defined(__EMSCRIPTEN__)
                pthread_t* thread = static_cast<pthread_t*>(mThread);
                pthread_join(*thread, NULL);
                delete thread;
                #endif
                mThread = nullptr;
            }
        }
        
        bool NkFileWatcher::IsWatching() const {
            return mIsWatching;
        }
        
        void NkFileWatcher::SetPath(const char* path) {
            bool wasWatching = mIsWatching;
            if (wasWatching) Stop();
            
            mPath = path;
            
            if (wasWatching) Start();
        }
        
        void NkFileWatcher::SetPath(const NkPath& path) {
            SetPath(path.CStr());
        }
        
        void NkFileWatcher::SetCallback(NkFileWatcherCallback* callback) {
            mCallback = callback;
        }
        
        void NkFileWatcher::SetRecursive(bool recursive) {
            bool wasWatching = mIsWatching;
            if (wasWatching) Stop();
            
            mRecursive = recursive;
            
            if (wasWatching) Start();
        }
        
        const NkString& NkFileWatcher::GetPath() const {
            return mPath;
        }
        
        bool NkFileWatcher::IsRecursive() const {
            return mRecursive;
        }
        
    } // namespace entseu
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
