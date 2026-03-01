// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/DistributingSink.h
// DESCRIPTION: Sink qui distribue les messages à plusieurs sous-sinks.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Sink.h"
#include <vector>
#include <memory>
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
// CLASSE: DistributingSink
// DESCRIPTION: Sink qui distribue les messages à plusieurs sous-sinks
// -------------------------------------------------------------------------
class LOGGER_API DistributingSink : public ISink {
public:
	// ---------------------------------------------------------------------
	// CONSTRUCTEURS
	// ---------------------------------------------------------------------

	/**
	 * @brief Constructeur par défaut
	 */
	DistributingSink();

	/**
	 * @brief Constructeur avec liste initiale de sinks
	 * @param sinks Liste de sinks à ajouter
	 */
	explicit DistributingSink(const std::vector<std::shared_ptr<ISink>> &sinks);

	/**
	 * @brief Destructeur
	 */
	~DistributingSink() override;

	// ---------------------------------------------------------------------
	// IMPLÉMENTATION DE ISink
	// ---------------------------------------------------------------------

	/**
	 * @brief Distribue un message à tous les sous-sinks
	 */
	void Log(const LogMessage &message) override;

	/**
	 * @brief Force le flush de tous les sous-sinks
	 */
	void Flush() override;

	/**
	 * @brief Définit le formatter pour tous les sous-sinks
	 */
	void SetFormatter(std::unique_ptr<Formatter> formatter) override;

	/**
	 * @brief Définit le pattern de formatage pour tous les sous-sinks
	 */
	void SetPattern(const std::string &pattern) override;

	/**
	 * @brief Obtient le formatter (du premier sink)
	 */
	Formatter *GetFormatter() const override;

	/**
	 * @brief Obtient le pattern (du premier sink)
	 */
	std::string GetPattern() const override;

	// ---------------------------------------------------------------------
	// GESTION DES SOUS-SINKS
	// ---------------------------------------------------------------------

	/**
	 * @brief Ajoute un sous-sink
	 * @param sink Sink à ajouter
	 */
	void AddSink(std::shared_ptr<ISink> sink);

	/**
	 * @brief Supprime un sous-sink
	 * @param sink Sink à supprimer
	 */
	void RemoveSink(std::shared_ptr<ISink> sink);

	/**
	 * @brief Supprime tous les sous-sinks
	 */
	void ClearSinks();

	/**
	 * @brief Obtient la liste des sous-sinks
	 * @return Vecteur des sous-sinks
	 */
	std::vector<std::shared_ptr<ISink>> GetSinks() const;

	/**
	 * @brief Obtient le nombre de sous-sinks
	 * @return Nombre de sous-sinks
	 */
	size_t GetSinkCount() const;

	/**
	 * @brief Vérifie si un sink spécifique est présent
	 * @param sink Sink à rechercher
	 * @return true si présent, false sinon
	 */
	bool ContainsSink(std::shared_ptr<ISink> sink) const;

private:
	// ---------------------------------------------------------------------
	// VARIABLES MEMBRE PRIVÉES
	// ---------------------------------------------------------------------

	/// Liste des sous-sinks
	std::vector<std::shared_ptr<ISink>> m_Sinks;

	/// Mutex pour la synchronisation thread-safe
	mutable std::mutex m_Mutex;
};

} // namespace logger
} // namespace nkentseu