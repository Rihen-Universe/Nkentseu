#pragma once

// =============================================================================
// NkGamepadSystem.h
// Système gamepad / joystick cross-platform.
//
// Architecture :
//   NkGamepadSystem        — singleton public (polling + callbacks)
//   INkGamepadBackend      — interface PIMPL par plateforme
//
// Backends disponibles (implémentations concrètes) :
//   NkWin32GamepadBackend   — XInput (Xbox) + DirectInput HID (Win32)
//   NkCocoaGamepadBackend   — IOKit HID / GCController (macOS)
//   NkUIKitGamepadBackend   — GCController (iOS / tvOS)
//   NkAndroidGamepadBackend — android/input.h AInputEvent (Android)
//   NkLinuxGamepadBackend   — evdev /dev/input/event* (Linux)
//   NkWASMGamepadBackend    — Gamepad Web API (WASM / navigateur)
//   NkNoopGamepadBackend    — stub sans opération (headless / tests)
//
// Intégration avec EventSystem :
//   PollGamepads() détecte les deltas de boutons et d'axes par rapport à
//   l'état précédent et émet les événements NkGamepad* correspondants via
//   EventSystem::DispatchEvent().
//   EventSystem::PollEvents() appelle PollGamepads() automatiquement si
//   SetAutoGamepadPoll(true) (activé par défaut).
//
// Usage minimal :
//   // Init (appelé par NkSystem::Initialise) :
//   NkGamepadSystem::Instance().Init();
//
//   // Callbacks :
//   NkGamepads().SetConnectCallback([](const NkGamepadInfo& info, bool connected) {
//       Log("Gamepad", info.name, connected ? "connectée" : "déconnectée");
//   });
//
//   // Boucle principale :
//   NkGamepads().PollGamepads();
//
//   // Polling direct (ne génère pas d'événements) :
//   if (NkGamepads().IsButtonDown(0, NkGamepadButton::NK_GP_SOUTH)) Fire();
//   float lx = NkGamepads().GetAxis(0, NkGamepadAxis::NK_GP_AXIS_LX);
//
//   // Vibration :
//   NkGamepads().Rumble(0, 0.5f, 0.3f, 0.f, 0.f, 200);
// =============================================================================

#include "NkGamepadEvent.h"
#include "NkEventState.h"

#include <functional>
#include <memory>
#include <array>
#include <cstring>

namespace nkentseu {

	// ---------------------------------------------------------------------------
	// Constante
	// ---------------------------------------------------------------------------

	/// @brief Nombre maximal de manettes supportées simultanément
	inline constexpr NkU32 NK_MAX_GAMEPADS = 8;

	// ---------------------------------------------------------------------------
	// Snapshot complet d'une manette (pour le polling direct)
	// ---------------------------------------------------------------------------

	/**
	 * @brief Snapshot complet de l'état d'une manette à un instant donné.
	 *
	 * Différent de NkGamepadInputState (dans NkEventState.h) qui est mis à jour
	 * par les événements. NkGamepadSnapshot est fourni directement par le backend
	 * à chaque appel de Poll(), avant le calcul des deltas.
	 */
	struct NkGamepadSnapshot {
		bool          connected   = false;
		NkGamepadInfo info;

		// Boutons — indexés par NkGamepadButton (valeur NkU32)
		bool buttons[static_cast<NkU32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX)] = {};

		// Axes analogiques — indexés par NkGamepadAxis (valeur NkU32)
		NkF32 axes[static_cast<NkU32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX)] = {};

		// Capteurs (si disponibles)
		NkF32 gyroX = 0.f, gyroY = 0.f, gyroZ = 0.f;     ///< [rad/s]
		NkF32 accelX = 0.f, accelY = 0.f, accelZ = 0.f;   ///< [m/s²]

		// Batterie
		NkF32 batteryLevel = -1.f;  ///< [0,1] ou -1 (filaire / inconnu)
		bool  isCharging   = false;

		bool IsButtonDown(NkGamepadButton b) const noexcept {
			NkU32 idx = static_cast<NkU32>(b);
			return (idx < static_cast<NkU32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX))
				&& buttons[idx];
		}

		NkF32 GetAxis(NkGamepadAxis a) const noexcept {
			NkU32 idx = static_cast<NkU32>(a);
			return (idx < static_cast<NkU32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX))
				? axes[idx] : 0.f;
		}

		void Clear() noexcept {
			connected = false;
			info      = {};
			std::memset(buttons, 0, sizeof(buttons));
			std::memset(axes,    0, sizeof(axes));
			gyroX = gyroY = gyroZ   = 0.f;
			accelX = accelY = accelZ = 0.f;
			batteryLevel = -1.f;
			isCharging   = false;
		}
	};

	// ---------------------------------------------------------------------------
	// Callbacks
	// ---------------------------------------------------------------------------

	using NkGamepadConnectCallback = std::function<void(const NkGamepadInfo&, bool connected)>;
	using NkGamepadButtonCallback  = std::function<void(NkU32 idx, NkGamepadButton, NkButtonState)>;
	using NkGamepadAxisCallback    = std::function<void(NkU32 idx, NkGamepadAxis,   NkF32 value)>;

	// ===========================================================================
	// INkGamepadBackend — interface PIMPL du backend manette
	// ===========================================================================

	/**
	 * @brief Interface pure pour le backend de gestion des manettes.
	 *
	 * Chaque plateforme fournit une implémentation concrète. Le backend est
	 * créé par NkGamepadSystem::Init() via une factory selon la plateforme compilée.
	 */
	class INkGamepadBackend {
		public:
			virtual ~INkGamepadBackend() = default;

			// -----------------------------------------------------------------------
			// Cycle de vie
			// -----------------------------------------------------------------------

			/**
			 * @brief Initialise le backend (ouvre les descripteurs, enregistre les
			 *        callbacks OS, configure XInput…).
			 * @return true si l'initialisation a réussi.
			 */
			virtual bool Init() = 0;

			/**
			 * @brief Libère toutes les ressources (ferme les descripteurs, supprime
			 *        les callbacks OS…).
			 */
			virtual void Shutdown() = 0;

			// -----------------------------------------------------------------------
			// Polling
			// -----------------------------------------------------------------------

			/**
			 * @brief Pompe les événements manette et met à jour les snapshots internes.
			 *
			 * Cette méthode ne génère PAS d'événements NkGamepad* — c'est
			 * NkGamepadSystem::PollGamepads() qui compare avec l'état précédent
			 * et émet les événements.
			 */
			virtual void Poll() = 0;

			// -----------------------------------------------------------------------
			// Accès à l'état courant
			// -----------------------------------------------------------------------

			/// @brief Nombre de manettes actuellement connectées
			virtual NkU32 GetConnectedCount() const = 0;

			/**
			 * @brief Snapshot complet de la manette à l'indice idx.
			 * @param idx  Indice de la manette (0-based).
			 */
			virtual const NkGamepadSnapshot& GetSnapshot(NkU32 idx) const = 0;

			// -----------------------------------------------------------------------
			// Commandes de sortie
			// -----------------------------------------------------------------------

			/**
			 * @brief Lance une vibration sur la manette idx.
			 *
			 * Les valeurs sont dans [0,1]. L'implémentation ignore silencieusement
			 * si la manette ne supporte pas la vibration.
			 *
			 * @param idx          Indice de la manette.
			 * @param motorLow     Moteur basse fréquence [0,1].
			 * @param motorHigh    Moteur haute fréquence [0,1].
			 * @param triggerLeft  Gâchette gauche [0,1] (DualSense, Xbox Elite).
			 * @param triggerRight Gâchette droite [0,1].
			 * @param durationMs   Durée [ms], 0 = jusqu'à l'appel Stop suivant.
			 */
			virtual void Rumble(NkU32 idx,
								NkF32 motorLow, NkF32 motorHigh,
								NkF32 triggerLeft, NkF32 triggerRight,
								NkU32 durationMs) = 0;

			/**
			 * @brief Définit la couleur de la LED d'une manette.
			 *
			 * @param idx   Indice de la manette.
			 * @param rgba  Couleur RGBA 0xRRGGBBAA (ex: 0xFF0000FF = rouge opaque).
			 *
			 * @note  Supporté par DualSense, DualShock 4, Joy-Con.
			 *        Ignoré silencieusement si non supporté.
			 */
			virtual void SetLEDColor(NkU32 idx, NkU32 rgba) {
				(void)idx; (void)rgba;
			}

			/**
			 * @brief Retourne true si la manette dispose de capteurs inertiels
			 *        (gyroscope + accéléromètre).
			 */
			virtual bool HasMotion(NkU32 idx) const noexcept {
				(void)idx; return false;
			}

			/**
			 * @brief Retourne le nom du backend (pour diagnostics).
			 * @return ex: "XInput", "evdev", "GCController", "WebGamepad"
			 */
			virtual const char* GetName() const noexcept = 0;
	};

	// ===========================================================================
	// NkGamepadSystem — façade singleton
	// ===========================================================================

	/**
	 * @brief Système de gestion des manettes cross-platform.
	 *
	 * Gère le cycle de vie du backend, maintient les snapshots par slot,
	 * détecte les deltas boutons/axes et émet les événements NkGamepad* via
	 * EventSystem::DispatchEvent().
	 */
	class NkGamepadSystem {
		public:
			// =========================================================================
			// Singleton
			// =========================================================================

			static NkGamepadSystem& Instance();

			NkGamepadSystem(const NkGamepadSystem&)            = delete;
			NkGamepadSystem& operator=(const NkGamepadSystem&) = delete;

			// =========================================================================
			// Cycle de vie
			// =========================================================================

			/**
			 * @brief Initialise le système et le backend plateforme.
			 *
			 * Crée automatiquement le backend approprié selon la plateforme.
			 * Peut être appelé avec un backend externe (tests, mocking).
			 *
			 * @param backend  Backend personnalisé (nullptr = factory automatique).
			 * @return true si l'initialisation a réussi.
			 */
			bool Init(std::unique_ptr<INkGamepadBackend> backend = nullptr);

			/// @brief Libère le backend et l'état interne
			void Shutdown();

			bool IsReady() const noexcept { return mReady && mBackend != nullptr; }

			// =========================================================================
			// Pompe (appeler chaque trame dans la boucle principale)
			// =========================================================================

			/**
			 * @brief Pompe le backend, détecte les deltas et émet les événements.
			 *
			 * Séquence pour chaque slot manette :
			 *   1. Appelle mBackend->Poll()
			 *   2. Compare le snapshot courant avec mPrevSnapshot[idx]
			 *   3. Pour chaque bouton changé : émet NkGamepadButtonPressEvent ou
			 *      NkGamepadButtonReleaseEvent via EventSystem::DispatchEvent()
			 *   4. Pour chaque axe ayant bougé au-delà d'un epsilon : émet
			 *      NkGamepadAxisEvent
			 *   5. Met à jour mPrevSnapshot[idx]
			 *   6. Appelle les callbacks directs mButtonCb, mAxisCb
			 *   7. Met à jour NkEventState dans EventSystem si activé
			 *
			 * Appelé automatiquement par EventSystem::PollEvents() si
			 * SetAutoGamepadPoll(true) (défaut).
			 */
			void PollGamepads();

			// =========================================================================
			// Callbacks directs (alternatif aux événements)
			// =========================================================================

			/**
			 * @brief Callback déclenché lors de la connexion / déconnexion d'une manette.
			 * @code
			 *   NkGamepads().SetConnectCallback([](const NkGamepadInfo& info, bool connected) {
			 *       Log(info.name, connected);
			 *   });
			 * @endcode
			 */
			void SetConnectCallback(NkGamepadConnectCallback cb) { mConnectCb = std::move(cb); }

			/**
			 * @brief Callback déclenché pour chaque changement d'état de bouton.
			 */
			void SetButtonCallback(NkGamepadButtonCallback cb) { mButtonCb = std::move(cb); }

			/**
			 * @brief Callback déclenché pour chaque mouvement d'axe significatif.
			 */
			void SetAxisCallback(NkGamepadAxisCallback cb) { mAxisCb = std::move(cb); }

			// =========================================================================
			// Polling direct (ne génère pas d'événements)
			// =========================================================================

			/// @brief Nombre de manettes connectées
			NkU32 GetConnectedCount() const noexcept;

			/// @brief Retourne true si la manette idx est connectée
			bool IsConnected(NkU32 idx) const noexcept;

			/**
			 * @brief Informations sur la manette idx.
			 * @return Référence vers des données vides si idx est invalide.
			 */
			const NkGamepadInfo& GetInfo(NkU32 idx) const noexcept;

			/**
			 * @brief Snapshot complet de la manette idx.
			 * @return Référence vers un snapshot vide si idx est invalide.
			 */
			const NkGamepadSnapshot& GetSnapshot(NkU32 idx) const noexcept;

			/// @brief Retourne true si le bouton btn est enfoncé sur la manette idx
			bool IsButtonDown(NkU32 idx, NkGamepadButton btn) const noexcept;

			/**
			 * @brief Valeur de l'axe ax sur la manette idx.
			 * @return Valeur normalisée [-1,1] ou [0,1] selon l'axe, 0 si invalide.
			 */
			NkF32 GetAxis(NkU32 idx, NkGamepadAxis ax) const noexcept;

			// =========================================================================
			// Commandes de sortie
			// =========================================================================

			/**
			 * @brief Lance une vibration sur la manette idx.
			 *
			 * @param idx          Indice du joueur (0-based).
			 * @param motorLow     Moteur basse fréquence [0,1].
			 * @param motorHigh    Moteur haute fréquence [0,1].
			 * @param triggerLeft  Gâchette gauche [0,1].
			 * @param triggerRight Gâchette droite [0,1].
			 * @param durationMs   Durée [ms], 0 = infini jusqu'à Rumble(0,0,0,0).
			 */
			void Rumble(NkU32 idx,
						NkF32 motorLow     = 0.f,
						NkF32 motorHigh    = 0.f,
						NkF32 triggerLeft  = 0.f,
						NkF32 triggerRight = 0.f,
						NkU32 durationMs   = 100);

			/// @brief Arrête toutes les vibrations de la manette idx
			void RumbleStop(NkU32 idx) { Rumble(idx, 0.f, 0.f, 0.f, 0.f, 0); }

			/// @brief Définit la couleur LED de la manette idx (0xRRGGBBAA)
			void SetLEDColor(NkU32 idx, NkU32 rgba);

			// =========================================================================
			// Configuration
			// =========================================================================

			/**
			 * @brief Zone morte appliquée par PollGamepads() avant émission d'un
			 *        NkGamepadAxisEvent.
			 *
			 * Si |value| < deadzone, l'axe est forcé à 0 et aucun événement n'est
			 * émis.
			 * @param deadzone  Valeur dans [0, 0.5] (défaut 0.08).
			 */
			void SetDeadzone(NkF32 deadzone) noexcept { mDeadzone = deadzone; }
			NkF32 GetDeadzone() const noexcept { return mDeadzone; }

			/**
			 * @brief Seuil de changement minimum sur un axe pour émettre un événement.
			 * @param epsilon  Valeur dans [0, 0.1] (défaut 0.001).
			 */
			void SetAxisEpsilon(NkF32 epsilon) noexcept { mAxisEpsilon = epsilon; }
			NkF32 GetAxisEpsilon() const noexcept { return mAxisEpsilon; }

			// =========================================================================
			// Accès backend
			// =========================================================================

			INkGamepadBackend* GetBackend() noexcept { return mBackend.get(); }
			const INkGamepadBackend* GetBackend() const noexcept { return mBackend.get(); }

		private:
			NkGamepadSystem() = default;

			void FireConnect(const NkGamepadInfo& info, bool connected);
			void FireButton(NkU32 idx, NkGamepadButton btn, NkButtonState state);
			void FireAxis(NkU32 idx, NkGamepadAxis ax, NkF32 value, NkF32 prevValue);

			NkF32 ApplyDeadzone(NkF32 value) const noexcept {
				if (value >  mDeadzone) return value;
				if (value < -mDeadzone) return value;
				return 0.f;
			}

			std::unique_ptr<INkGamepadBackend> mBackend;
			bool mReady = false;

			// Callbacks directs
			NkGamepadConnectCallback mConnectCb;
			NkGamepadButtonCallback  mButtonCb;
			NkGamepadAxisCallback    mAxisCb;

			// Snapshots précédents pour détection de delta
			std::array<NkGamepadSnapshot, NK_MAX_GAMEPADS> mPrevSnapshot;

			// Configuration
			NkF32 mDeadzone    = 0.08f;
			NkF32 mAxisEpsilon = 0.001f;

			// Sentinelles pour les accès invalides
			static NkGamepadSnapshot sDummySnapshot;
			static NkGamepadInfo     sDummyInfo;
	};

	// ---------------------------------------------------------------------------
	// Raccourci global
	// ---------------------------------------------------------------------------

	/// @brief Accès raccourci au singleton NkGamepadSystem
	inline NkGamepadSystem& NkGamepads() {
		return NkGamepadSystem::Instance();
	}

} // namespace nkentseu