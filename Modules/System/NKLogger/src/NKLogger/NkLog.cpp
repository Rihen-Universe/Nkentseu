// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkLog.cpp
// DESCRIPTION: Implémentation du logger par défaut avec API fluide.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/NkLog.h"
#include "NKLogger/Sinks/NkConsoleSink.h"
#include "NKLogger/Sinks/NkFileSink.h"
#include <cstdarg>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// INITIALISATION DES VARIABLES STATIQUES
	// -------------------------------------------------------------------------

	bool NkLog::s_Initialized = false;

	// -------------------------------------------------------------------------
	// IMPLÉMENTATION DE NkLog
	// -------------------------------------------------------------------------

	/**
	 * @brief Constructeur privé
	 */
	NkLog::NkLog(const std::string &name) : NkLogger(name) {

		// Ajouter un sink console par défaut avec couleurs
		auto consoleSink = std::make_shared<NkConsoleSink>();
		consoleSink->SetColorEnabled(true);
		AddSink(consoleSink);

		// Ajouter un sink fichier par défaut
		auto fileSink = std::make_shared<NkFileSink>("logs/app.log");
		AddSink(fileSink);

		// Configuration par défaut
		SetLevel(NkLogLevel::NK_INFO);
		SetPattern(NkFormatter::NK_NKENTSEU_PATTERN);
	}

	/**
	 * @brief Destructeur
	 */
	NkLog::~NkLog() {
		Flush();
	}

	/**
	 * @brief Obtient l'instance singleton
	 */
	NkLog &NkLog::Instance() {
		static NkLog instance;
		s_Initialized = true;
		return instance;
	}

	/**
	 * @brief Initialise le logger par défaut
	 */
	void NkLog::Initialize(const std::string &name, const std::string &pattern, NkLogLevel level) {
		auto &instance = Instance();

		if (instance.GetName() != name && !name.empty()) {
			instance.SetName(name);
		}

		instance.SetPattern(pattern);
		instance.SetLevel(level);
		s_Initialized = true;
	}

	/**
	 * @brief Nettoie le logger par défaut
	 */
	void NkLog::Shutdown() {
		auto &instance = Instance();
		instance.Flush();
		instance.ClearSinks();
	}

	/**
	 * @brief Configure les informations de source
	 */
	// SourceContext NkLog::Source(const char* file, int line, const char* function) const {
	//     return SourceContext(file, line, function);
	// }

	/**
	 * @brief Configure le nom du logger
	 */
	NkLog &NkLog::Named(const std::string &name) {
		SetName(name);
		return *this;
	}

	/**
	 * @brief Configure le niveau de log
	 */
	NkLog &NkLog::Level(NkLogLevel level) {
		SetLevel(level);
		return *this;
	}

	/**
	 * @brief Configure le pattern
	 */
	NkLog &NkLog::Pattern(const std::string &pattern) {
		SetPattern(pattern);
		return *this;
	}

	NkLog &NkLog::Source(const char *sourceFile, uint32 sourceLine, const char *functionName) {
		NkLogger::Source(sourceFile, sourceLine, functionName);
		return *this;
	}

} // namespace nkentseu
