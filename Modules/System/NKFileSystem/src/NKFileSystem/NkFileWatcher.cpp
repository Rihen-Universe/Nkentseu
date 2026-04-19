// =============================================================================
// FICHIER  : Modules/System/NKFileSystem/src/NKFileSystem/NkFileWatcher.cpp
// MODULE   : NKFileSystem
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Implementation de la surveillance de changements de fichiers.
// =============================================================================

#include "NKFileSystem/NkFileWatcher.h"

// -- En-tetes C standard ------------------------------------------------------
#include <cstring>
#include <ctime>

// -- En-tetes plateforme ------------------------------------------------------
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#elif defined(__EMSCRIPTEN__)
    // WebAssembly: pas d'API noyau de file watching.
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/inotify.h>
    #include <sys/select.h>
    #include <errno.h>
#endif

namespace nkentseu {

    // =========================================================================
    // Structures
    // =========================================================================

    NkFileChangeEvent::NkFileChangeEvent()
        : Type(NkFileChangeType::NK_MODIFIED)
        , Path()
        , OldPath()
        , Timestamp(0) {
    }

    NkFileChangeEvent::NkFileChangeEvent(NkFileChangeType type, const char* path)
        : Type(type)
        , Path(path ? path : "")
        , OldPath()
        , Timestamp(static_cast<nk_int64>(time(nullptr))) {
    }

    // =========================================================================
    // Constructeurs
    // =========================================================================

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
        , mPath(path ? path : "")
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
        DWORD bytesReturned = 0;
        OVERLAPPED overlapped = {};
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        while (mIsWatching) {
            BOOL success = ReadDirectoryChangesW(
                hDir,
                buffer,
                BUFFER_SIZE,
                mRecursive ? TRUE : FALSE,
                FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_ATTRIBUTES |
                FILE_NOTIFY_CHANGE_SIZE |
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
                        char filename[MAX_PATH] = {};

                        WideCharToMultiByte(
                            CP_UTF8,
                            0,
                            info->FileName,
                            info->FileNameLength / static_cast<int>(sizeof(WCHAR)),
                            filename,
                            MAX_PATH,
                            NULL,
                            NULL
                        );

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
                            default:
                                event.Type = NkFileChangeType::NK_MODIFIED;
                                break;
                        }

                        mCallback->OnFileChanged(event);

                        if (info->NextEntryOffset == 0) {
                            break;
                        }

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

        const uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB;

        int wd = inotify_add_watch(fd, mPath.CStr(), mask);
        if (wd < 0) {
            close(fd);
            return;
        }

        const size_t eventSize = sizeof(struct inotify_event);
        const size_t bufferLen = 1024 * (eventSize + 16);
        char buffer[1024 * (sizeof(struct inotify_event) + 16)];

        while (mIsWatching) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            int ret = select(fd + 1, &fds, NULL, NULL, &tv);

            if (ret < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }

            if (ret == 0) {
                continue;
            }

            ssize_t length = read(fd, buffer, bufferLen);
            if (length < 0) {
                break;
            }

            if (!mCallback) {
                continue;
            }

            size_t i = 0;
            while (i < static_cast<size_t>(length)) {
                struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);

                if (event->len > 0) {
                    NkFileChangeEvent changeEvent;
                    changeEvent.Path = (NkPath(mPath.CStr()) / event->name).ToString();
                    changeEvent.Timestamp = static_cast<nk_int64>(time(nullptr));

                    if (event->mask & IN_CREATE) {
                        changeEvent.Type = NkFileChangeType::NK_CREATED;
                    } else if (event->mask & IN_DELETE) {
                        changeEvent.Type = NkFileChangeType::NK_DELETED;
                    } else if (event->mask & IN_MODIFY) {
                        changeEvent.Type = NkFileChangeType::NK_MODIFIED;
                    } else if (event->mask & (IN_MOVED_FROM | IN_MOVED_TO)) {
                        changeEvent.Type = NkFileChangeType::NK_RENAMED;
                    } else if (event->mask & IN_ATTRIB) {
                        changeEvent.Type = NkFileChangeType::NK_ATTRIBUTE_CHANGED;
                    } else {
                        changeEvent.Type = NkFileChangeType::NK_MODIFIED;
                    }

                    mCallback->OnFileChanged(changeEvent);
                }

                i += eventSize + event->len;
            }
        }

        inotify_rm_watch(fd, wd);
        close(fd);
    }

#endif

    // =========================================================================
    // Controle
    // =========================================================================

    bool NkFileWatcher::Start() {
        if (mIsWatching) {
            return true;
        }

        if (mPath.Empty() || !mCallback) {
            return false;
        }

        mIsWatching = true;

#ifdef _WIN32
        mThread = reinterpret_cast<void*>(_beginthread(
            reinterpret_cast<void(*)(void*)>(ThreadProc),
            0,
            this
        ));
#elif defined(__EMSCRIPTEN__)
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
        if (!mIsWatching) {
            return;
        }

        mIsWatching = false;

        if (!mThread) {
            return;
        }

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

    bool NkFileWatcher::IsWatching() const {
        return mIsWatching;
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    void NkFileWatcher::SetPath(const char* path) {
        const bool wasWatching = mIsWatching;

        if (wasWatching) {
            Stop();
        }

        mPath = (path ? path : "");

        if (wasWatching) {
            Start();
        }
    }

    void NkFileWatcher::SetPath(const NkPath& path) {
        SetPath(path.CStr());
    }

    void NkFileWatcher::SetCallback(NkFileWatcherCallback* callback) {
        mCallback = callback;
    }

    void NkFileWatcher::SetRecursive(bool recursive) {
        const bool wasWatching = mIsWatching;

        if (wasWatching) {
            Stop();
        }

        mRecursive = recursive;

        if (wasWatching) {
            Start();
        }
    }

    const NkString& NkFileWatcher::GetPath() const {
        return mPath;
    }

    bool NkFileWatcher::IsRecursive() const {
        return mRecursive;
    }

} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
