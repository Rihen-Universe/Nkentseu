#pragma once
// =============================================================================
// NkPty.h — Pseudo-terminal (ConPTY) : shell INTERACTIF persistant facon VSCode.
//   Contrairement a NkProcess (un _popen jetable par commande), NkPty lance UN
//   shell (powershell / wsl -d <distro> / bash / cmd) attache a un pseudo-console
//   Windows (ConPTY). On ECRIT les frappes clavier dans son entree et on LIT en
//   continu sa sortie (avec sequences VT : couleurs, deplacement curseur, clear).
//   => le shell affiche lui-meme son invite ; `clear` efface ; pas de boite de
//      saisie separee. La sortie brute est interpretee par l'emulateur NkTerm.
//
//   L'implementation Win32 (windows.h, CreatePseudoConsole...) est ISOLEE dans
//   NkPty.cpp pour ne pas polluer les en-tetes GUI avec les macros de windows.h.
// =============================================================================
#include "NKThreading/NkThread.h"
#include "NKThreading/NkMutex.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
namespace nkcode {

    using namespace nkentseu;

    class NkPty {
    public:
        NkPty() = default;
        ~NkPty();
        NkPty(const NkPty&)            = delete;
        NkPty& operator=(const NkPty&) = delete;

        // Lance `cmdline` (ex. "powershell.exe", "wsl.exe -d Ubuntu-22.04") attache
        // a un ConPTY de taille cols x rows. false si echec / ConPTY indisponible.
        bool Start(const NkString& cmdline, int16 cols, int16 rows);

        // Ecrit des octets bruts (UTF-8) dans l'entree du shell (frappes clavier).
        void Write(const char* data, usize len);
        void Write(const char* s);

        // Redimensionne le pseudo-console (a appeler quand le panneau change de taille).
        void Resize(int16 cols, int16 rows);

        // Recupere (et vide) la sortie accumulee depuis le dernier appel. Thread-safe.
        void Drain(NkVector<char>& out);

        // Arrete le shell + le thread de lecture + libere les handles.
        void Stop();

        bool  Running() const { return mRunning; }
        int16 Cols()    const { return mCols; }
        int16 Rows()    const { return mRows; }

    private:
        void ReadLoop();   // boucle du thread de lecture (impl. Win32 dans le .cpp)

        // Handles Win32 opaques (void* pour ne pas exposer windows.h ici).
        void* mInWrite  = nullptr;   // HANDLE : on ecrit dedans
        void* mOutRead  = nullptr;   // HANDLE : on lit dedans
        void* mProcess  = nullptr;   // HANDLE process du shell
        void* mPC       = nullptr;   // HPCON (pseudo-console)
        void* mResizeFn = nullptr;   // ResizePseudoConsole (charge dynamiquement)
        void* mCloseFn  = nullptr;   // ClosePseudoConsole

        threading::NkThread mThread;
        threading::NkMutex  mMutex;
        NkVector<char>      mBuf;     // sortie brute (protege par mMutex)
        volatile bool       mRunning = false;
        int16               mCols = 80;
        int16               mRows = 24;
    };

} // namespace nkcode
} // namespace nkentseu
