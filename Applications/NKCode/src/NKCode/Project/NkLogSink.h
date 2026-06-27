#pragma once
// =============================================================================
// NkLogSink.h — Sink NKLogger -> panneau OUTPUT de NKCode.
//   Un NkISink custom capture chaque message de log (niveau + texte) dans un
//   tampon THREAD-SAFE ; le panneau OUTPUT le draine chaque frame et l'affiche
//   (vrai affichage NKLogger des logs du moteur).
// =============================================================================
#include "NKLogger/NkLog.h"
#include "NKLogger/NkSink.h"
#include "NKLogger/NkLogMessage.h"
#include "NKLogger/NkLogLevel.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NkSharedPtr.h"
#include "NKMemory/NkUniquePtr.h"

namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::threading;   // NkMutex / NkScopedLock

    // Tampon de logs partage (le sink ecrit depuis n'importe quel thread, l'UI draine).
    struct NkLogBuffer {
        NkMutex            mutex;
        NkVector<NkString> lines;
        void Push(const NkString& s) {
            threading::NkScopedLock<NkMutex> lk(mutex);
            lines.PushBack(s);
            if (lines.Size() > 5000) lines.Erase(lines.Begin());   // borne memoire
        }
        void Drain(NkVector<NkString>& out) {
            threading::NkScopedLock<NkMutex> lk(mutex);
            for (usize i = 0; i < lines.Size(); ++i) out.PushBack(lines[i]);
            lines.Clear();
        }
    };
    inline NkLogBuffer& GlobalLogBuffer() { static NkLogBuffer b; return b; }

    // Sink : route chaque message vers le tampon. Le reste de l'interface NkISink
    // est minimal (pas de formatter/pattern propre).
    class NkOutputSink : public NkISink {
    public:
        void Log(const NkLogMessage& m) override {
            NkString line("[");
            line += NkLogLevelToString(m.level);
            line += "] ";
            line += m.message.CStr();
            GlobalLogBuffer().Push(line);
        }
        void Flush() override {}
        void SetFormatter(memory::NkUniquePtr<NkLoggerFormatter>) override {}
        void SetPattern(const NkString&) override {}
        NkLoggerFormatter* GetFormatter() const override { return nullptr; }
        NkString GetPattern() const override { return NkString(); }
    };

    // A appeler au demarrage : branche le sink sur le logger global. NkSharedPtr
    // n'offre pas de conversion derive->base -> on passe par un pointeur base brut
    // (le sink vit toute la duree de l'app).
    inline void InstallLogSink() {
        NkISink* raw = memory::NkMakeUnique<NkOutputSink>().Release();
        NkLog::Instance().AddSink(memory::NkSharedPtr<NkISink>(raw));
    }

} // namespace nkcode
