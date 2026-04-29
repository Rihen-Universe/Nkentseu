// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkRegistry.cpp
// DESCRIPTION: Implémentation du registre global des loggers.
// Auteur: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/NkRegistry.h"
#include "NKLogger/Sinks/NkConsoleSink.h"

#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

    namespace {
        using NkLoggerEntry = NkPair<NkString, memory::NkSharedPtr<NkLogger>>;

        static usize NkFindLoggerIndex(const NkVector<NkLoggerEntry>& loggers, const NkString& name) {
            for (usize i = 0; i < loggers.Size(); ++i) {
                if (loggers[i].First == name) {
                    return i;
                }
            }
            return static_cast<usize>(-1);
        }
    } // namespace

    // -------------------------------------------------------------------------
    // IMPLÉMENTATION DE NkRegistry
    // -------------------------------------------------------------------------

    /**
     * @brief Constructeur privé
     */
    NkRegistry::NkRegistry()
        : m_GlobalLevel(NkLogLevel::NK_INFO), m_GlobalPattern(NkFormatter::NK_DEFAULT_PATTERN), m_Initialized(false) {
    }

    /**
     * @brief Destructeur privé
     */
    NkRegistry::~NkRegistry() {
        Clear();
    }

    /**
     * @brief Obtient l'instance singleton du registre
     */
    NkRegistry &NkRegistry::Instance() {
        static NkRegistry instance;
        return instance;
    }

    /**
     * @brief Initialise le registre avec des paramètres par défaut
     */
    void NkRegistry::Initialize() {
        auto &instance = Instance();

        threading::NkScopedLockMutex lock(instance.m_Mutex);
        if (!instance.m_Initialized) {
            instance.CreateDefaultLogger();
            instance.m_Initialized = true;
        }
    }

    /**
     * @brief Nettoie le registre (détruit tous les loggers)
     */
    void NkRegistry::Shutdown() {
        auto &instance = Instance();
        instance.Clear();
        instance.m_Initialized = false;
    }

    /**
     * @brief Enregistre un logger dans le registre
     */
    bool NkRegistry::Register(memory::NkSharedPtr<NkLogger> logger) {
        if (!logger.IsValid())
            return false;

        threading::NkScopedLockMutex lock(m_Mutex);
        NkString name = logger->GetName();
        if (NkFindLoggerIndex(m_Loggers, name) != static_cast<usize>(-1)) {
            return false; // Nom déjà existant
        }

        m_Loggers.PushBack(NkLoggerEntry(name, logger));
        return true;
    }

    /**
     * @brief Désenregistre un logger du registre
     */
    bool NkRegistry::Unregister(const NkString &name) {
        threading::NkScopedLockMutex lock(m_Mutex);
        const usize index = NkFindLoggerIndex(m_Loggers, name);
        if (index != static_cast<usize>(-1)) {
            m_Loggers.Erase(m_Loggers.begin() + index);
            return true;
        }

        return false;
    }

    /**
     * @brief Obtient un logger par son nom
     */
    memory::NkSharedPtr<NkLogger> NkRegistry::Get(const NkString &name) {
        threading::NkScopedLockMutex lock(m_Mutex);
        const usize index = NkFindLoggerIndex(m_Loggers, name);
        if (index != static_cast<usize>(-1)) {
            return m_Loggers[index].Second;
        }

        return memory::NkSharedPtr<NkLogger>();
    }

    /**
     * @brief Obtient un logger par son nom (crée si non existant)
     */
    memory::NkSharedPtr<NkLogger> NkRegistry::GetOrCreate(const NkString &name) {
        threading::NkScopedLockMutex lock(m_Mutex);
        const usize index = NkFindLoggerIndex(m_Loggers, name);
        if (index != static_cast<usize>(-1)) {
            return m_Loggers[index].Second;
        }

        // Création d'un nouveau logger
        auto logger = memory::NkMakeShared<NkLogger>(name);
        logger->SetLevel(m_GlobalLevel);
        logger->SetPattern(m_GlobalPattern);

        m_Loggers.PushBack(NkLoggerEntry(name, logger));
        return logger;
    }

    /**
     * @brief Vérifie si un logger existe
     */
    bool NkRegistry::Exists(const NkString &name) const {
        threading::NkScopedLockMutex lock(m_Mutex);
        return NkFindLoggerIndex(m_Loggers, name) != static_cast<usize>(-1);
    }

    /**
     * @brief Supprime tous les loggers du registre
     */
    void NkRegistry::Clear() {
        threading::NkScopedLockMutex lock(m_Mutex);
        m_Loggers.Clear();
        m_DefaultLogger.Reset();
    }

    /**
     * @brief Obtient la liste de tous les noms de loggers
     */
    NkVector<NkString> NkRegistry::GetLoggerNames() const {
        threading::NkScopedLockMutex lock(m_Mutex);
        NkVector<NkString> names;
        names.Reserve(m_Loggers.Size());

        for (const auto &pair : m_Loggers) {
            names.PushBack(pair.First);
        }

        return names;
    }

    /**
     * @brief Obtient le nombre de loggers enregistrés
     */
    usize NkRegistry::GetLoggerCount() const {
        threading::NkScopedLockMutex lock(m_Mutex);
        return m_Loggers.Size();
    }

    /**
     * @brief Définit le niveau de log global
     */
    void NkRegistry::SetGlobalLevel(NkLogLevel level) {
        threading::NkScopedLockMutex lock(m_Mutex);
        m_GlobalLevel = level;

        // Appliquer à tous les loggers existants
        for (auto &pair : m_Loggers) {
            if (pair.Second.IsValid()) {
                pair.Second->SetLevel(level);
            }
        }
    }

    /**
     * @brief Obtient le niveau de log global
     */
    NkLogLevel NkRegistry::GetGlobalLevel() const {
        threading::NkScopedLockMutex lock(m_Mutex);
        return m_GlobalLevel;
    }

    /**
     * @brief Définit le pattern global
     */
    void NkRegistry::SetGlobalPattern(const NkString &pattern) {
        threading::NkScopedLockMutex lock(m_Mutex);
        m_GlobalPattern = pattern;

        // Appliquer à tous les loggers existants
        for (auto &pair : m_Loggers) {
            if (pair.Second.IsValid()) {
                pair.Second->SetPattern(pattern);
            }
        }
    }

    /**
     * @brief Obtient le pattern global
     */
    NkString NkRegistry::GetGlobalPattern() const {
        threading::NkScopedLockMutex lock(m_Mutex);
        return m_GlobalPattern;
    }

    /**
     * @brief Force le flush de tous les loggers
     */
    void NkRegistry::FlushAll() {
        threading::NkScopedLockMutex lock(m_Mutex);

        for (auto &pair : m_Loggers) {
            if (pair.Second.IsValid()) {
                pair.Second->Flush();
            }
        }
    }

    /**
     * @brief Définit le logger par défaut
     */
    void NkRegistry::SetDefaultLogger(memory::NkSharedPtr<NkLogger> logger) {
        threading::NkScopedLockMutex lock(m_Mutex);
        m_DefaultLogger = logger;

        // S'assurer qu'il est aussi dans le registre
        if (logger.IsValid() && NkFindLoggerIndex(m_Loggers, logger->GetName()) == static_cast<usize>(-1)) {
            m_Loggers.PushBack(NkLoggerEntry(logger->GetName(), logger));
        }
    }

    /**
     * @brief Obtient le logger par défaut
     */
    memory::NkSharedPtr<NkLogger> NkRegistry::GetDefaultLogger() {
        {
            threading::NkScopedLockMutex lock(m_Mutex);
            if (m_DefaultLogger.IsValid()) {
                return m_DefaultLogger;
            }
        }
        return CreateDefaultLogger();
    }

    /**
     * @brief Crée un logger par défaut avec console sink
     */
    memory::NkSharedPtr<NkLogger> NkRegistry::CreateDefaultLogger() {
        auto logger = memory::NkMakeShared<NkLogger>("default");

            logger->SetLevel(m_GlobalLevel);
            logger->SetPattern(m_GlobalPattern);

            // Ajouter un sink console par défaut
            memory::NkSharedPtr<NkISink> consoleSink(new NkConsoleSink());
            logger->AddSink(consoleSink);

        // Enregistrer le logger
        Register(logger);
        m_DefaultLogger = logger;

        return logger;
    }

    // -------------------------------------------------------------------------
    // FONCTIONS UTILITAIRES GLOBALES
    // -------------------------------------------------------------------------

    /**
     * @brief Obtient un logger par son nom
     */
    memory::NkSharedPtr<NkLogger> GetLogger(const NkString &name) {
        return NkRegistry::Instance().Get(name);
    }

    /**
     * @brief Obtient le logger par défaut
     */
    memory::NkSharedPtr<NkLogger> GetDefaultLogger() {
        return NkRegistry::Instance().GetDefaultLogger();
    }

    /**
     * @brief Crée un logger avec un nom spécifique
     */
    memory::NkSharedPtr<NkLogger> CreateLogger(const NkString &name) {
        auto &registry = NkRegistry::Instance();
        return registry.GetOrCreate(name);
    }

    /**
     * @brief Supprime tous les loggers
     */
    void DropAll() {
        NkRegistry::Instance().Clear();
    }

    /**
     * @brief Supprime un logger spécifique
     */
    void Drop(const NkString &name) {
        NkRegistry::Instance().Unregister(name);
    }
} // namespace nkentseu
