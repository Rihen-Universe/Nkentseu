#pragma once

// =============================================================================
// NkSystem.h
// Point d'entrée global du framework NkWindow.
//
//   NkInitialise(data);  ← initialise plateforme + event system
//   NkClose();           ← libère tout proprement
//
// NkAppData permet de configurer le renderer par défaut, le debug, etc.
// On n'a plus besoin de passer IEventImpl aux constructeurs de Window.
// =============================================================================

#include "NkTypes.h"
#include "NkPlatformDetect.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NkSurface.h"
#include <string>
#include <memory>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

class IEventImpl;

// ---------------------------------------------------------------------------
// NkAppData — paramètres de démarrage de l'application
// ---------------------------------------------------------------------------

struct NkAppData {
	// --- Renderer préféré ---
	NkRendererApi preferredRenderer = NkRendererApi::NK_SOFTWARE;

	// --- Debug ---
	bool enableRendererDebug = false;
	bool enableEventLogging = false;

	// --- Application ---
	std::string appName = "NkApp";
	std::string appVersion = "1.0.0";

	// --- Divers ---
	bool enableMultiWindow = true; ///< Permettre plusieurs fenêtres
};

// ---------------------------------------------------------------------------
// NkSystem — gestion du cycle de vie global
// ---------------------------------------------------------------------------

class NkSystem {
public:
	NkSystem() = default;
	~NkSystem() = default;

	NkSystem(const NkSystem &) = delete;
	NkSystem &operator=(const NkSystem &) = delete;

	static NkSystem &Instance();

	// -----------------------------------------------------------------------
	// Initialise la plateforme et le système d'événements.
	// Doit être appelé AVANT toute création de Window.
	// -----------------------------------------------------------------------
	bool Initialise(const NkAppData &data = {});

	// -----------------------------------------------------------------------
	// Libère toutes les ressources (ferme fenêtres, event impl, etc.)
	// -----------------------------------------------------------------------
	void Close();

	// -----------------------------------------------------------------------
	// Accès
	// -----------------------------------------------------------------------
	bool IsInitialised() const {
		return mInitialised;
	}

	IEventImpl *GetEventImpl() const {
		return NkEventSystem::Instance().GetImpl();
	}

	// -----------------------------------------------------------------------
	// SINGLETON EVENT IMPL — Règle d'architecture
	// -----------------------------------------------------------------------
	// Il existe UNE SEULE instance de IEventImpl par NkSystem (et donc
	// par application).  Toutes les fenêtres s'y enregistrent via
	// eventImpl.Initialize(this, nativeHandle).
	// Plusieurs instances de Window sont supportées.
	// -----------------------------------------------------------------------
	const NkAppData &GetAppData() const {
		return mAppData;
	}

private:
	bool mInitialised = false;
	NkAppData mAppData;
};

// ---------------------------------------------------------------------------
// Fonctions globales de commodité
// ---------------------------------------------------------------------------

/**
 * @brief Initialise le framework NkWindow.
 *        Doit être appelé une fois en début de programme (ou dans nkmain()).
 *
 * @code
 *   nkentseu::NkAppData data;
 *   data.preferredRenderer = nkentseu::NkRendererApi::NK_OPENGL;
 *   nkentseu::NkInitialise(data);
 * @endcode
 */
inline bool NkInitialise(const NkAppData &data = {}) {
	return NkSystem::Instance().Initialise(data);
}

/**
 * @brief Libère toutes les ressources du framework.
 *        Appeler avant la fin de nkmain() / main().
 */
inline void NkClose() {
	NkSystem::Instance().Close();
}

/**
 * @brief Accède à l'implémentation d'événements active (plateforme courante).
 */
inline IEventImpl *NkGetEventImpl() {
	return NkSystem::Instance().GetEventImpl();
}

} // namespace nkentseu
