// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/ConsoleSink.h
// DESCRIPTION: Sink pour la sortie console (stdout/stderr) avec support couleur.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Sink.h"
#include <iostream>
#include <mutex>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// ÉNUMÉRATION: ConsoleStream
// DESCRIPTION: Flux de console disponibles
// -------------------------------------------------------------------------
enum class ConsoleStream {
	StdOut, // Sortie standard (stdout)
	StdErr	// Erreur standard (stderr)
};

// -------------------------------------------------------------------------
// CLASSE: ConsoleSink
// DESCRIPTION: Sink pour la sortie console avec couleurs
// -------------------------------------------------------------------------
class LOGGER_API ConsoleSink : public ISink {
public:
	// ---------------------------------------------------------------------
	// CONSTRUCTEURS
	// ---------------------------------------------------------------------

	/**
	 * @brief Constructeur par défaut (stdout, avec couleurs)
	 */
	ConsoleSink();

	/**
	 * @brief Constructeur avec flux spécifique
	 * @param stream Flux à utiliser
	 * @param useColors Utiliser les couleurs (si supporté)
	 */
	explicit ConsoleSink(ConsoleStream stream, bool useColors = true);

	/**
	 * @brief Destructeur
	 */
	~ConsoleSink() override;

	// ---------------------------------------------------------------------
	// IMPLÉMENTATION DE ISink
	// ---------------------------------------------------------------------

	/**
	 * @brief Logge un message dans la console
	 */
	void Log(const LogMessage &message) override;

	/**
	 * @brief Force l'écriture des données en attente
	 */
	void Flush() override;

	/**
	 * @brief Définit le formatter pour ce sink
	 */
	void SetFormatter(std::unique_ptr<Formatter> formatter) override;

	/**
	 * @brief Définit le pattern de formatage
	 */
	void SetPattern(const std::string &pattern) override;

	/**
	 * @brief Obtient le formatter courant
	 */
	Formatter *GetFormatter() const override;

	/**
	 * @brief Obtient le pattern courant
	 */
	std::string GetPattern() const override;

	// ---------------------------------------------------------------------
	// CONFIGURATION SPÉCIFIQUE À LA CONSOLE
	// ---------------------------------------------------------------------

	/**
	 * @brief Active ou désactive les couleurs
	 * @param enable true pour activer les couleurs
	 */
	void SetColorEnabled(bool enable);

	/**
	 * @brief Vérifie si les couleurs sont activées
	 * @return true si activées, false sinon
	 */
	bool IsColorEnabled() const;

	/**
	 * @brief Définit le flux de console
	 * @param stream Flux à utiliser
	 */
	void SetStream(ConsoleStream stream);

	/**
	 * @brief Obtient le flux de console courant
	 * @return Flux courant
	 */
	ConsoleStream GetStream() const;

	/**
	 * @brief Définit si le sink doit utiliser stderr pour les niveaux d'erreur
	 * @param enable true pour utiliser stderr pour Error/Critical/Fatal
	 */
	void SetUseStderrForErrors(bool enable);

	/**
	 * @brief Vérifie si le sink utilise stderr pour les erreurs
	 * @return true si activé, false sinon
	 */
	bool IsUsingStderrForErrors() const;

private:
	// ---------------------------------------------------------------------
	// MÉTHODES PRIVÉES
	// ---------------------------------------------------------------------

	/**
	 * @brief Obtient le flux de sortie approprié pour un niveau de log
	 * @param level Niveau de log
	 * @return Référence au flux ostream approprié
	 */
	std::ostream &GetStreamForLevel(LogLevel level);

	/**
	 * @brief Vérifie si la console supporte les couleurs
	 * @return true si supporté, false sinon
	 */
	bool SupportsColors() const;

	/**
	 * @brief Obtient le code couleur pour un niveau de log
	 * @param level Niveau de log
	 * @return Code couleur ANSI
	 */
	std::string GetColorCode(LogLevel level) const;

	/**
	 * @brief Obtient le code de réinitialisation de couleur
	 * @return Code de réinitialisation ANSI
	 */
	std::string GetResetCode() const;

	/**
	 * @brief Configure la couleur Windows pour un niveau de log
	 * @param level Niveau de log
	 */
	void SetWindowsColor(LogLevel level);

	/**
	 * @brief Réinitialise la couleur Windows
	 */
	void ResetWindowsColor();

	// ---------------------------------------------------------------------
	// VARIABLES MEMBRE PRIVÉES
	// ---------------------------------------------------------------------

	/// Formatter pour ce sink
	std::unique_ptr<Formatter> m_Formatter;

	/// Flux de console principal
	ConsoleStream m_Stream;

	/// Utiliser les couleurs
	bool m_UseColors;

	/// Utiliser stderr pour les niveaux d'erreur
	bool m_UseStderrForErrors;

	/// Mutex pour la synchronisation thread-safe
	mutable std::mutex m_Mutex;
};

} // namespace logger
} // namespace nkentseu