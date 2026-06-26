#pragma once
// =============================================================================
// NkProcess.h — Lancement de processus externe ASYNCHRONE avec capture de sortie.
//   Start(cmd) lance la commande sur un THREAD d'arriere-plan (stdout+stderr
//   fusionnes) ; le thread pousse chaque ligne dans une file THREAD-SAFE ;
//   l'UI appelle Drain() chaque frame pour recuperer les nouvelles lignes sans
//   geler. Base sur NKThreading (NkThread/NkMutex) + _popen.
//
//   v1 header-only, local a NKCode (build async + terminal). Promouvable en
//   module partage Kernel/System/NKProcess quand un autre consommateur en aura
//   besoin (debug, agents...). Limite : _popen ne permet pas de tuer le process.
// =============================================================================
#include "NKThreading/NkThread.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>

namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::threading;   // NkThread / NkMutex / NkScopedLock

    // Retire les sequences ANSI (couleurs ESC[...m) + \r\n d'une ligne de sortie.
    inline void NkStripAnsiInto(const char* s, char* dst, usize cap) {
        usize j = 0;
        for (const char* p = s; *p && j + 1 < cap; ++p) {
            if (*p == 0x1b && p[1] == '[') {            // CSI : ESC '[' params final
                p += 2;
                while (*p && !(*p >= '@' && *p <= '~')) ++p;   // params jusqu'a l'octet final
                if (!*p) break;                          // (le ++p de la boucle saute l'octet final)
            } else if (*p == '\r' || *p == '\n') {
                // saute les fins de ligne (chaque fgets = 1 ligne)
            } else {
                dst[j++] = *p;
            }
        }
        dst[j] = '\0';
    }

    class NkProcess {
    public:
        NkProcess() = default;
        ~NkProcess() { if (mThread.Joinable()) mThread.Join(); }

        NkProcess(const NkProcess&)            = delete;
        NkProcess& operator=(const NkProcess&) = delete;

        // Lance `command` (stderr fusionne) en arriere-plan. false si deja en cours.
        bool Start(const NkString& command) {
            if (mRunning) return false;
            if (mThread.Joinable()) mThread.Join();   // recycle un thread precedent termine
            mCmd     = command;
            mRunning = true;
            mDone    = false;
            mExit    = 0;
            mThread  = NkThread([this](void*) { Run(); });
            return true;
        }

        // Recupere (et vide) les lignes accumulees depuis le dernier appel. Thread-safe.
        void Drain(NkVector<NkString>& out) {
            threading::NkScopedLock<NkMutex> lk(mMutex);
            for (usize i = 0; i < mLines.Size(); ++i) out.PushBack(mLines[i]);
            mLines.Clear();
        }

        bool Running()  const { return mRunning; }
        bool Done()     const { return mDone; }
        int  ExitCode() const { return mExit; }

    private:
        void Run() {
            NkString c = mCmd; c += " 2>&1";
#if defined(_WIN32)
            FILE* pipe = _popen(c.CStr(), "r");
#else
            FILE* pipe = popen(c.CStr(), "r");
#endif
            if (!pipe) { Push(NkString("[erreur] impossible de lancer la commande")); mExit = -1; mDone = true; mRunning = false; return; }
            char line[2048], clean[2048];
            while (std::fgets(line, sizeof(line), pipe)) { NkStripAnsiInto(line, clean, sizeof(clean)); Push(NkString(clean)); }
#if defined(_WIN32)
            mExit = _pclose(pipe);
#else
            mExit = pclose(pipe);
#endif
            mDone    = true;
            mRunning = false;
        }

        void Push(const NkString& s) {
            threading::NkScopedLock<NkMutex> lk(mMutex);
            mLines.PushBack(s);
        }

        NkThread           mThread;
        NkMutex            mMutex;
        NkVector<NkString> mLines;     // protege par mMutex
        NkString           mCmd;
        bool               mRunning = false;   // ecrit par le thread, lu (poll) par l'UI
        bool               mDone    = false;
        int                mExit    = 0;
    };

} // namespace nkcode
