#include "pch.h"
#include "NKLogger/NkSink.h"

namespace nkentseu {

    NkISink::~NkISink() {}

    void NkISink::SetLevel(NkLogLevel level) {
        m_Level = level;
    }

    NkLogLevel NkISink::GetLevel() const {
        return m_Level;
    }

    bool NkISink::ShouldLog(NkLogLevel level) const {
        return m_Enabled && (static_cast<int>(level) >= static_cast<int>(m_Level));
    }

    void NkISink::SetEnabled(bool enabled) {
        m_Enabled = enabled;
    }

    bool NkISink::IsEnabled() const {
        return m_Enabled;
    }

    NkString NkISink::GetName() const {
        return m_Name;
    }

    void NkISink::SetName(const NkString& name) {
        m_Name = name;
    }

} // namespace nkentseu
