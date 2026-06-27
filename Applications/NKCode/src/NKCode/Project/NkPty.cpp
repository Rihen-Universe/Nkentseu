// =============================================================================
// NkPty.cpp — Implementation Win32 (ConPTY) de NkPty. windows.h confine ici.
// =============================================================================
#include "NKCode/Project/NkPty.h"
#include "NKThreading/NkScopedLock.h"

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>

  // PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE (peut manquer sur de vieux SDK).
  #ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
  #define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
  #endif

  // Signatures ConPTY (chargees dynamiquement depuis kernel32 -> robuste).
  typedef HRESULT (WINAPI *PFN_CreatePseudoConsole)(COORD, HANDLE, HANDLE, DWORD, void**);
  typedef HRESULT (WINAPI *PFN_ResizePseudoConsole)(void*, COORD);
  typedef void    (WINAPI *PFN_ClosePseudoConsole)(void*);
#endif

namespace nkcode {

    using namespace nkentseu::threading;

    NkPty::~NkPty() { Stop(); }

    bool NkPty::Start(const NkString& cmdline, int16 cols, int16 rows) {
#if defined(_WIN32)
        if (mRunning) return false;
        mCols = cols < 1 ? 80 : cols;
        mRows = rows < 1 ? 24 : rows;

        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        if (!k32) return false;
        PFN_CreatePseudoConsole pCreate = (PFN_CreatePseudoConsole)GetProcAddress(k32, "CreatePseudoConsole");
        PFN_ResizePseudoConsole pResize = (PFN_ResizePseudoConsole)GetProcAddress(k32, "ResizePseudoConsole");
        PFN_ClosePseudoConsole  pClose  = (PFN_ClosePseudoConsole) GetProcAddress(k32, "ClosePseudoConsole");
        if (!pCreate || !pClose) return false;   // ConPTY indisponible (< Win10 1809)
        mResizeFn = (void*)pResize;
        mCloseFn  = (void*)pClose;

        // Tubes : entree (inRead -> ConPTY, mInWrite -> nous) ; sortie (mOutRead ->
        // nous, outWrite -> ConPTY). Handles non heritables (sa == NULL).
        HANDLE inRead = NULL, inWrite = NULL, outRead = NULL, outWrite = NULL;
        if (!CreatePipe(&inRead, &inWrite, NULL, 0)) return false;
        if (!CreatePipe(&outRead, &outWrite, NULL, 0)) { CloseHandle(inRead); CloseHandle(inWrite); return false; }

        COORD size; size.X = mCols; size.Y = mRows;
        void* hpc = NULL;
        HRESULT hr = pCreate(size, inRead, outWrite, 0, &hpc);
        // ConPTY a duplique ce dont il a besoin : on rend nos copies inutiles.
        CloseHandle(inRead);
        CloseHandle(outWrite);
        if (FAILED(hr) || !hpc) { CloseHandle(inWrite); CloseHandle(outRead); return false; }
        mPC = hpc; mInWrite = inWrite; mOutRead = outRead;

        // STARTUPINFOEX + attribut PSEUDOCONSOLE.
        STARTUPINFOEXW si; ZeroMemory(&si, sizeof(si));
        si.StartupInfo.cb = sizeof(si);
        SIZE_T attrSize = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &attrSize);
        si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrSize);
        bool attrOk = si.lpAttributeList
            && InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrSize)
            && UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                         mPC, sizeof(mPC), NULL, NULL);
        if (!attrOk) {
            if (si.lpAttributeList) HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
            pClose(mPC); mPC = NULL;
            CloseHandle(mInWrite); mInWrite = NULL;
            CloseHandle(mOutRead); mOutRead = NULL;
            return false;
        }

        // Ligne de commande mutable en UTF-16.
        int wn = MultiByteToWideChar(CP_UTF8, 0, cmdline.CStr(), -1, NULL, 0);
        if (wn <= 0) wn = 1;
        nkentseu::NkVector<wchar_t> wcmd; wcmd.Resize(static_cast<usize>(wn));
        if (MultiByteToWideChar(CP_UTF8, 0, cmdline.CStr(), -1, wcmd.Data(), wn) <= 0) wcmd[0] = 0;

        PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
        BOOL ok = CreateProcessW(NULL, wcmd.Data(), NULL, NULL, FALSE,
                                 EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                                 &si.StartupInfo, &pi);
        DeleteProcThreadAttributeList(si.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        if (!ok) {
            pClose(mPC); mPC = NULL;
            CloseHandle(mInWrite); mInWrite = NULL;
            CloseHandle(mOutRead); mOutRead = NULL;
            return false;
        }
        mProcess = pi.hProcess;
        CloseHandle(pi.hThread);

        mRunning = true;
        mThread = NkThread([this](void*) { ReadLoop(); });
        return true;
#else
        (void)cmdline; (void)cols; (void)rows;
        return false;
#endif
    }

    void NkPty::ReadLoop() {
#if defined(_WIN32)
        char buf[4096];
        DWORD rd = 0;
        for (;;) {
            BOOL ok = ReadFile((HANDLE)mOutRead, buf, sizeof(buf), &rd, NULL);
            if (!ok || rd == 0) break;
            threading::NkScopedLock<NkMutex> lk(mMutex);
            for (DWORD i = 0; i < rd; ++i) mBuf.PushBack(buf[i]);
        }
        mRunning = false;
#endif
    }

    void NkPty::Write(const char* data, usize len) {
#if defined(_WIN32)
        if (!mRunning || !mInWrite || !data || len == 0) return;
        DWORD wr = 0;
        WriteFile((HANDLE)mInWrite, data, static_cast<DWORD>(len), &wr, NULL);
#else
        (void)data; (void)len;
#endif
    }

    void NkPty::Write(const char* s) {
        if (!s) return;
        usize n = 0; while (s[n]) ++n;
        Write(s, n);
    }

    void NkPty::Resize(int16 cols, int16 rows) {
#if defined(_WIN32)
        if (!mRunning || !mPC || !mResizeFn) return;
        if (cols < 1) cols = 1; if (rows < 1) rows = 1;
        if (cols == mCols && rows == mRows) return;
        mCols = cols; mRows = rows;
        COORD c; c.X = cols; c.Y = rows;
        ((PFN_ResizePseudoConsole)mResizeFn)(mPC, c);
#else
        (void)cols; (void)rows;
#endif
    }

    void NkPty::Drain(NkVector<char>& out) {
        threading::NkScopedLock<NkMutex> lk(mMutex);
        for (usize i = 0; i < mBuf.Size(); ++i) out.PushBack(mBuf[i]);
        mBuf.Clear();
    }

    void NkPty::Stop() {
#if defined(_WIN32)
        // 1) EOF sur stdin du shell -> il commence a sortir.
        if (mInWrite) { CloseHandle((HANDLE)mInWrite); mInWrite = NULL; }
        // 2) Ferme le pseudo-console -> termine le client, debloque ReadFile.
        if (mPC && mCloseFn) { ((PFN_ClosePseudoConsole)mCloseFn)(mPC); mPC = NULL; }
        // 3) Le thread de lecture sort (ReadFile renvoie 0) -> on le joint.
        if (mThread.Joinable()) mThread.Join();
        // 4) Libere le reste.
        if (mOutRead) { CloseHandle((HANDLE)mOutRead); mOutRead = NULL; }
        if (mProcess) { TerminateProcess((HANDLE)mProcess, 0); CloseHandle((HANDLE)mProcess); mProcess = NULL; }
        mRunning = false;
#endif
    }

} // namespace nkcode
