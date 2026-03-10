#pragma once

// =============================================================================
// NkGamepadSystem.h
// SystÃ¨me gamepad / joystick cross-platform.
//
// Architecture :
//   NkGamepadSystem  â€” possÃ©dÃ© par NkSystem (plus de singleton autoproclamÃ©)
//   NkIGamepad       â€” interface PIMPL par plateforme
//
// CORRECTION 1 : NkGamepadSystem n'est plus un singleton.
//   Avant : NkGamepadSystem::Instance().Init()   (Meyer singleton cachÃ©)
//   AprÃ¨s : NkSystem::Instance().GetGamepadSystem()  (propriÃ©tÃ© explicite)
//   Le raccourci NkGamepads() dÃ©lÃ¨gue Ã  NkSystem::Gamepads() (dÃ©fini dans NkSystem.h).
//
// Backends disponibles (implÃ©mentations concrÃ¨tes) :
//   NkWin32Gamepad    â€” XInput (Xbox) + DirectInput HID (Win32)
//   NkCocoaGamepad    â€” IOKit HID / GCController (macOS)
//   NkUIKitGamepad    â€” GCController (iOS / tvOS)
//   NkAndroidGamepad  â€” android/input.h AInputEvent (Android)
//   NkLinuxGamepad    â€” evdev /dev/input/event* (Linux)
//   NkEmscriptenGamepad     â€” Gamepad Web API (WASM / navigateur)
//   NkNoopGamepad     â€” stub sans opÃ©ration (headless / fallback)
//
// IntÃ©gration avec EventSystem :
//   PollGamepads() dÃ©tecte les deltas de boutons et d'axes par rapport Ã 
//   l'Ã©tat prÃ©cÃ©dent et Ã©met les Ã©vÃ©nements NkGamepad* correspondants via
//   EventSystem::DispatchEvent().
//   EventSystem::PollEvents() appelle PollGamepads() automatiquement si
//   SetAutoGamepadPoll(true) (activÃ© par dÃ©faut).
//
// Usage minimal :
//   // Init (appelÃ© automatiquement par NkSystem::Initialise) :
//   NkSystem::Instance().Initialise();
//
//   // Callbacks :
//   NkGamepads().SetConnectCallback([](const NkGamepadInfo& info, bool connected) {
//       Log("Gamepad", info.name, connected ? "connectÃ©e" : "dÃ©connectÃ©e");
//   });
//
//   // Boucle principale :
//   NkGamepads().PollGamepads();
//
//   // Polling direct (ne gÃ©nÃ¨re pas d'Ã©vÃ©nements) :
//   if (NkGamepads().IsButtonDown(0, NkGamepadButton::NK_GP_SOUTH)) Fire();
//   float lx = NkGamepads().GetAxis(0, NkGamepadAxis::NK_GP_AXIS_LX);
//
//   // Vibration :
//   NkGamepads().Rumble(0, 0.5f, 0.3f, 0.f, 0.f, 200);
// =============================================================================

#include "NkGamepadEvent.h"
#include "NkEventState.h"
#include "NkGamepadMappingPersistence.h"

#include <functional>
#include <memory>
#include <array>
#include <cstring>
#include <string>
#include <cmath>

namespace nkentseu {

	// ---------------------------------------------------------------------------
	// Constante
	// ---------------------------------------------------------------------------

	/// @brief Nombre maximal de manettes supportÃ©es simultanÃ©ment
	inline constexpr NkU32 NK_MAX_GAMEPADS = 8;
	/// @brief Valeur sentinelle pour dÃ©sactiver un mapping bouton/axe.
	inline constexpr NkU32 NK_GAMEPAD_UNMAPPED = 0xFFFFFFFFu;
	/// @brief CapacitÃ© de mapping boutons (physiques/logiques) demandÃ©e.
	inline constexpr NkU32 NK_GAMEPAD_BUTTON_MAPPING_CAPACITY = 102;
	/// @brief CapacitÃ© de mapping axes (physiques/logiques) demandÃ©e.
	inline constexpr NkU32 NK_GAMEPAD_AXIS_MAPPING_CAPACITY = 54;

	// ---------------------------------------------------------------------------
	// Snapshot complet d'une manette (pour le polling direct)
	// ---------------------------------------------------------------------------

	/**
	 * @brief Snapshot complet de l'Ã©tat d'une manette Ã  un instant donnÃ©.
	 *
	 * DiffÃ©rent de NkGamepadInputState (dans NkEventState.h) qui est mis Ã  jour
	 * par les Ã©vÃ©nements. NkGamepadSnapshot est fourni directement par le backend
	 * Ã  chaque appel de Poll(), avant le calcul des deltas.
	 */
	struct NkGamepadSnapshot {
		bool          connected   = false;
		NkGamepadInfo info;

		// Boutons â€” capacitÃ© Ã©tendue pour les pÃ©riphÃ©riques riches
		bool buttons[NK_GAMEPAD_BUTTON_MAPPING_CAPACITY] = {};

		// Axes analogiques â€” capacitÃ© Ã©tendue
		NkF32 axes[NK_GAMEPAD_AXIS_MAPPING_CAPACITY] = {};

		// Capteurs (si disponibles)
		NkF32 gyroX = 0.f, gyroY = 0.f, gyroZ = 0.f;     ///< [rad/s]
		NkF32 accelX = 0.f, accelY = 0.f, accelZ = 0.f;   ///< [m/sÂ²]

		// Batterie
		NkF32 batteryLevel = -1.f;  ///< [0,1] ou -1 (filaire / inconnu)
		bool  isCharging   = false;

		bool IsButtonDown(NkGamepadButton b) const noexcept {
			NkU32 idx = static_cast<NkU32>(b);
			return (idx < NK_GAMEPAD_BUTTON_MAPPING_CAPACITY)
				&& buttons[idx];
		}

		NkF32 GetAxis(NkGamepadAxis a) const noexcept {
			NkU32 idx = static_cast<NkU32>(a);
			return (idx < NK_GAMEPAD_AXIS_MAPPING_CAPACITY)
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
	// NkIGamepad â€” interface PIMPL du backend manette
	// ===========================================================================

	/**
	 * @brief Interface pure pour le backend de gestion des manettes.
	 *
	 * Chaque plateforme fournit une implÃ©mentation concrÃ¨te. Le backend est
	 * crÃ©Ã© par NkGamepadSystem::Init() via une factory selon la plateforme compilÃ©e.
	 */
	class NkIGamepad {
		public:
			virtual ~NkIGamepad() = default;

			// -----------------------------------------------------------------------
			// Cycle de vie
			// -----------------------------------------------------------------------

			/**
			 * @brief Initialise le backend (ouvre les descripteurs, enregistre les
			 *        callbacks OS, configure XInputâ€¦).
			 * @return true si l'initialisation a rÃ©ussi.
			 */
			virtual bool Init() = 0;

			/**
			 * @brief LibÃ¨re toutes les ressources (ferme les descripteurs, supprime
			 *        les callbacks OSâ€¦).
			 */
			virtual void Shutdown() = 0;

			// -----------------------------------------------------------------------
			// Polling
			// -----------------------------------------------------------------------

			/**
			 * @brief Pompe les Ã©vÃ©nements manette et met Ã  jour les snapshots internes.
			 *
			 * Cette mÃ©thode ne gÃ©nÃ¨re PAS d'Ã©vÃ©nements NkGamepad* â€” c'est
			 * NkGamepadSystem::PollGamepads() qui compare avec l'Ã©tat prÃ©cÃ©dent
			 * et Ã©met les Ã©vÃ©nements.
			 */
			virtual void Poll() = 0;

			// -----------------------------------------------------------------------
			// AccÃ¨s Ã  l'Ã©tat courant
			// -----------------------------------------------------------------------

			/// @brief Nombre de manettes actuellement connectÃ©es
			virtual NkU32 GetConnectedCount() const = 0;

			/**
			 * @brief Snapshot complet de la manette Ã  l'indice idx.
			 * @param idx  Indice de la manette (0-based).
			 */
			virtual const NkGamepadSnapshot& GetSnapshot(NkU32 idx) const = 0;

			// -----------------------------------------------------------------------
			// Commandes de sortie
			// -----------------------------------------------------------------------

			/**
			 * @brief Lance une vibration sur la manette idx.
			 *
			 * Les valeurs sont dans [0,1]. L'implÃ©mentation ignore silencieusement
			 * si la manette ne supporte pas la vibration.
			 *
			 * @param idx          Indice de la manette.
			 * @param motorLow     Moteur basse frÃ©quence [0,1].
			 * @param motorHigh    Moteur haute frÃ©quence [0,1].
			 * @param triggerLeft  GÃ¢chette gauche [0,1] (DualSense, Xbox Elite).
			 * @param triggerRight GÃ¢chette droite [0,1].
			 * @param durationMs   DurÃ©e [ms], 0 = jusqu'Ã  l'appel Stop suivant.
			 */
			virtual void Rumble(NkU32 idx,
								NkF32 motorLow, NkF32 motorHigh,
								NkF32 triggerLeft, NkF32 triggerRight,
								NkU32 durationMs) = 0;

			/**
			 * @brief DÃ©finit la couleur de la LED d'une manette.
			 *
			 * @param idx   Indice de la manette.
			 * @param rgba  Couleur RGBA 0xRRGGBBAA (ex: 0xFF0000FF = rouge opaque).
			 *
			 * @note  SupportÃ© par DualSense, DualShock 4, Joy-Con.
			 *        IgnorÃ© silencieusement si non supportÃ©.
			 */
			virtual void SetLEDColor(NkU32 idx, NkU32 rgba) {
				(void)idx; (void)rgba;
			}

			/**
			 * @brief Retourne true si la manette dispose de capteurs inertiels
			 *        (gyroscope + accÃ©lÃ©romÃ¨tre).
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
	// NkGamepadSystem â€” possÃ©dÃ© par NkSystem (CORRECTION 1 : plus de singleton)
	// ===========================================================================

	/**
	 * @brief SystÃ¨me de gestion des manettes cross-platform.
	 *
	 * GÃ¨re le cycle de vie du backend, maintient les snapshots par slot,
	 * dÃ©tecte les deltas boutons/axes et Ã©met les Ã©vÃ©nements NkGamepad* via
	 * EventSystem::DispatchEvent().
	 *
	 * CORRECTION 1 : plus de singleton â€” NkSystem en est le propriÃ©taire unique.
	 * AccÃ¨s via : NkSystem::Instance().GetGamepadSystem()  ou  NkGamepads()
	 */
	class NkGamepadSystem {
		public:
			static constexpr NkU32 BUTTON_COUNT       = NK_GAMEPAD_BUTTON_MAPPING_CAPACITY;
			static constexpr NkU32 AXIS_COUNT         = NK_GAMEPAD_AXIS_MAPPING_CAPACITY;
			static constexpr NkU32 EVENT_BUTTON_COUNT = static_cast<NkU32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
			static constexpr NkU32 EVENT_AXIS_COUNT   = static_cast<NkU32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);

			struct NkAxisRemap {
				NkU32 logicalAxis = NK_GAMEPAD_UNMAPPED;
				NkF32 scale       = 1.f;
				bool  invert      = false;
			};

			struct NkRemapProfile {
				bool active = false;
				std::array<NkU32, BUTTON_COUNT> buttonMap{};
				std::array<NkAxisRemap, AXIS_COUNT> axisMap{};
			};

			// PossÃ©dÃ© par NkSystem â€” constructeur public pour dÃ©claration membre valeur.
			NkGamepadSystem()  = default;
			~NkGamepadSystem() { if (mReady) Shutdown(); }

			NkGamepadSystem(const NkGamepadSystem&)            = delete;
			NkGamepadSystem& operator=(const NkGamepadSystem&) = delete;

			// =========================================================================
			// Cycle de vie
			// =========================================================================

			/**
			 * @brief Initialise le systÃ¨me et le backend plateforme.
			 *
			 * CrÃ©e automatiquement le backend appropriÃ© selon la plateforme.
			 * Peut Ãªtre appelÃ© avec un backend externe (tests, mocking).
			 *
			 * @param backend  Backend personnalisÃ© (nullptr = factory automatique).
			 * @return true si l'initialisation a rÃ©ussi.
			 */
			bool Init(std::unique_ptr<NkIGamepad> backend = nullptr);

			/// @brief LibÃ¨re le backend et l'Ã©tat interne
			void Shutdown();

			bool IsReady() const noexcept { return mReady && mBackend != nullptr; }

			// =========================================================================
			// Pompe (appeler chaque trame dans la boucle principale)
			// =========================================================================

			/**
			 * @brief Pompe le backend, dÃ©tecte les deltas et Ã©met les Ã©vÃ©nements.
			 *
			 * SÃ©quence pour chaque slot manette :
			 *   1. Appelle mBackend->Poll()
			 *   2. Compare le snapshot courant avec mPrevSnapshot[idx]
			 *   3. Pour chaque bouton changÃ© : Ã©met NkGamepadButtonPressEvent ou
			 *      NkGamepadButtonReleaseEvent via EventSystem::DispatchEvent()
			 *   4. Pour chaque axe ayant bougÃ© au-delÃ  d'un epsilon : Ã©met
			 *      NkGamepadAxisEvent
			 *   5. Met Ã  jour mPrevSnapshot[idx]
			 *   6. Appelle les callbacks directs mButtonCb, mAxisCb
			 *   7. Met Ã  jour NkEventState dans EventSystem si activÃ©
			 *
			 * AppelÃ© automatiquement par EventSystem::PollEvents() si
			 * SetAutoGamepadPoll(true) (dÃ©faut).
			 */
			void PollGamepads();

			// =========================================================================
			// Callbacks directs (alternatif aux Ã©vÃ©nements)
			// =========================================================================

			/**
			 * @brief Callback dÃ©clenchÃ© lors de la connexion / dÃ©connexion d'une manette.
			 * @code
			 *   NkGamepads().SetConnectCallback([](const NkGamepadInfo& info, bool connected) {
			 *       Log(info.name, connected);
			 *   });
			 * @endcode
			 */
			void SetConnectCallback(NkGamepadConnectCallback cb) { mConnectCb = std::move(cb); }

			/**
			 * @brief Callback dÃ©clenchÃ© pour chaque changement d'Ã©tat de bouton.
			 */
			void SetButtonCallback(NkGamepadButtonCallback cb) { mButtonCb = std::move(cb); }

			/**
			 * @brief Callback dÃ©clenchÃ© pour chaque mouvement d'axe significatif.
			 */
			void SetAxisCallback(NkGamepadAxisCallback cb) { mAxisCb = std::move(cb); }

			// =========================================================================
			// Polling direct (ne gÃ©nÃ¨re pas d'Ã©vÃ©nements)
			// =========================================================================

			/// @brief Nombre de manettes connectÃ©es
			NkU32 GetConnectedCount() const noexcept;

			/// @brief Retourne true si la manette idx est connectÃ©e
			bool IsConnected(NkU32 idx) const noexcept;

			/**
			 * @brief Informations sur la manette idx.
			 * @return RÃ©fÃ©rence vers des donnÃ©es vides si idx est invalide.
			 */
			const NkGamepadInfo& GetInfo(NkU32 idx) const noexcept;

			/**
			 * @brief Snapshot complet de la manette idx.
			 * @return RÃ©fÃ©rence vers un snapshot vide si idx est invalide.
			 */
			const NkGamepadSnapshot& GetSnapshot(NkU32 idx) const noexcept;

			/// @brief Retourne true si le bouton btn est enfoncÃ© sur la manette idx
			bool IsButtonDown(NkU32 idx, NkGamepadButton btn) const noexcept;
			bool IsButtonDownByIndex(NkU32 idx, NkU32 btnIndex) const noexcept;

			/**
			 * @brief Valeur de l'axe ax sur la manette idx.
			 * @return Valeur normalisÃ©e [-1,1] ou [0,1] selon l'axe, 0 si invalide.
			 */
			NkF32 GetAxis(NkU32 idx, NkGamepadAxis ax) const noexcept;
			NkF32 GetAxisByIndex(NkU32 idx, NkU32 axisIndex) const noexcept;

			/**
			 * @brief Snapshot brut (avant remapping utilisateur).
			 */
			const NkGamepadSnapshot& GetRawSnapshot(NkU32 idx) const noexcept;
			bool IsRawButtonDownByIndex(NkU32 idx, NkU32 btnIndex) const noexcept;
			NkF32 GetRawAxisByIndex(NkU32 idx, NkU32 axisIndex) const noexcept;

			// =========================================================================
			// Commandes de sortie
			// =========================================================================

			/**
			 * @brief Lance une vibration sur la manette idx.
			 *
			 * @param idx          Indice du joueur (0-based).
			 * @param motorLow     Moteur basse frÃ©quence [0,1].
			 * @param motorHigh    Moteur haute frÃ©quence [0,1].
			 * @param triggerLeft  GÃ¢chette gauche [0,1].
			 * @param triggerRight GÃ¢chette droite [0,1].
			 * @param durationMs   DurÃ©e [ms], 0 = infini jusqu'Ã  Rumble(0,0,0,0).
			 */
			void Rumble(NkU32 idx,
						NkF32 motorLow     = 0.f,
						NkF32 motorHigh    = 0.f,
						NkF32 triggerLeft  = 0.f,
						NkF32 triggerRight = 0.f,
						NkU32 durationMs   = 100);

			/// @brief ArrÃªte toutes les vibrations de la manette idx
			void RumbleStop(NkU32 idx) { Rumble(idx, 0.f, 0.f, 0.f, 0.f, 0); }

			/// @brief DÃ©finit la couleur LED de la manette idx (0xRRGGBBAA)
			void SetLEDColor(NkU32 idx, NkU32 rgba);

			// =========================================================================
			// Configuration
			// =========================================================================

			/**
			 * @brief Zone morte appliquÃ©e par PollGamepads() avant Ã©mission d'un
			 *        NkGamepadAxisEvent.
			 *
			 * Si |value| < deadzone, l'axe est forcÃ© Ã  0 et aucun Ã©vÃ©nement n'est
			 * Ã©mis.
			 * @param deadzone  Valeur dans [0, 0.5] (dÃ©faut 0.08).
			 */
			void SetDeadzone(NkF32 deadzone) noexcept {
				if (!std::isfinite(deadzone)) deadzone = 0.08f;
				if (deadzone < 0.f) deadzone = 0.f;
				if (deadzone > 0.95f) deadzone = 0.95f;
				mDeadzone = deadzone;
			}
			NkF32 GetDeadzone() const noexcept { return mDeadzone; }

			/**
			 * @brief Seuil de changement minimum sur un axe pour Ã©mettre un Ã©vÃ©nement.
			 * @param epsilon  Valeur dans [0, 0.1] (dÃ©faut 0.001).
			 */
			void SetAxisEpsilon(NkF32 epsilon) noexcept {
				if (!std::isfinite(epsilon)) epsilon = 0.001f;
				if (epsilon < 0.f) epsilon = 0.f;
				if (epsilon > 1.f) epsilon = 1.f;
				mAxisEpsilon = epsilon;
			}
			NkF32 GetAxisEpsilon() const noexcept { return mAxisEpsilon; }

			// =========================================================================
			// Remapping utilisateur (gamepad -> layout logique)
			// =========================================================================

			/**
			 * @brief Remet le slot en mapping identitÃ© (aucun remap).
			 */
			void ClearMapping(NkU32 idx) noexcept;

			/**
			 * @brief Remet tous les slots en mapping identitÃ©.
			 */
			void ClearAllMappings() noexcept;

			/**
			 * @brief Mappe un bouton physique (index backend) vers un bouton logique.
			 *        logicalButton = NK_GP_UNKNOWN dÃ©sactive ce bouton physique.
			 */
			void SetButtonMapByIndex(NkU32 idx,
									 NkU32 physicalButtonIndex,
									 NkGamepadButton logicalButton) noexcept;

			/**
			 * @brief Version typÃ©e de SetButtonMapByIndex.
			 */
			void SetButtonMap(NkU32 idx,
							  NkGamepadButton physicalButton,
							  NkGamepadButton logicalButton) noexcept
			{
				SetButtonMapByIndex(idx, static_cast<NkU32>(physicalButton), logicalButton);
			}

			/**
			 * @brief Mappe un axe physique vers un axe logique.
			 * @param invert Inverse le signe (utile pour Y).
			 * @param scale  Gain multiplicatif appliquÃ© Ã  l'axe.
			 */
			void SetAxisMapByIndex(NkU32 idx,
								   NkU32 physicalAxisIndex,
								   NkGamepadAxis logicalAxis,
								   bool invert = false,
								   NkF32 scale = 1.f) noexcept;

			/**
			 * @brief Version typÃ©e de SetAxisMapByIndex.
			 */
			void SetAxisMap(NkU32 idx,
							NkGamepadAxis physicalAxis,
							NkGamepadAxis logicalAxis,
							bool invert = false,
							NkF32 scale = 1.f) noexcept
			{
				SetAxisMapByIndex(idx, static_cast<NkU32>(physicalAxis), logicalAxis, invert, scale);
			}

			/**
			 * @brief DÃ©sactive un bouton physique (il n'alimente plus l'Ã©tat logique).
			 */
			void DisableButtonByIndex(NkU32 idx, NkU32 physicalButtonIndex) noexcept;

			/**
			 * @brief DÃ©sactive un axe physique.
			 */
			void DisableAxisByIndex(NkU32 idx, NkU32 physicalAxisIndex) noexcept;

			/**
			 * @brief AccÃ¨s lecture au profil de remap d'un slot.
			 */
			const NkRemapProfile* GetMapping(NkU32 idx) const noexcept;

			// =========================================================================
			// Persistance des profils de mapping (format configurable)
			// =========================================================================

			void SetMappingPersistence(std::unique_ptr<NkIGamepadMappingPersistence> persistence);
			NkIGamepadMappingPersistence* GetMappingPersistence() noexcept { return mMappingPersistence.get(); }
			const NkIGamepadMappingPersistence* GetMappingPersistence() const noexcept { return mMappingPersistence.get(); }

			NkGamepadMappingProfileData ExportMappingProfile() const;
			bool ImportMappingProfile(const NkGamepadMappingProfileData& profile,
									  bool clearExisting = true,
									  NkString* outError = nullptr);

			bool SaveMappingProfile(const NkString& userId = {},
									NkString* outError = nullptr) const;
			bool LoadMappingProfile(const NkString& userId = {},
									bool clearExisting = true,
									NkString* outError = nullptr);

			// =========================================================================
			// AccÃ¨s backend
			// =========================================================================

			NkIGamepad* GetBackend() noexcept { return mBackend.get(); }
			const NkIGamepad* GetBackend() const noexcept { return mBackend.get(); }

		private:
			void FireConnect(const NkGamepadInfo& info, bool connected);
			void FireButton(NkU32 idx, NkGamepadButton btn, NkButtonState state);
			void FireAxis(NkU32 idx, NkGamepadAxis ax, NkF32 value, NkF32 prevValue);
			void ResetMappingToIdentity(NkU32 idx) noexcept;
			void SyncMappedSnapshot(NkU32 idx) noexcept;
			const NkGamepadSnapshot& ApplyRemap(NkU32 idx, const NkGamepadSnapshot& raw) noexcept;
			static NkF32 ClampAxisForTarget(NkU32 logicalAxisIndex, NkF32 value) noexcept;

			NkF32 ApplyDeadzone(NkF32 value) const noexcept {
				if (!std::isfinite(value)) return 0.f;
				if (value >  mDeadzone) return value;
				if (value < -mDeadzone) return value;
				return 0.f;
			}

			std::unique_ptr<NkIGamepad> mBackend;
			bool mReady = false;

			// Callbacks directs
			NkGamepadConnectCallback mConnectCb;
			NkGamepadButtonCallback  mButtonCb;
			NkGamepadAxisCallback    mAxisCb;

			// Snapshots prÃ©cÃ©dents pour dÃ©tection de delta
			std::array<NkGamepadSnapshot, NK_MAX_GAMEPADS> mRawSnapshot;
			std::array<NkGamepadSnapshot, NK_MAX_GAMEPADS> mMappedSnapshot;
			std::array<NkGamepadSnapshot, NK_MAX_GAMEPADS> mPrevSnapshot;
			std::array<NkRemapProfile,   NK_MAX_GAMEPADS> mMappings;

			// Configuration
			NkF32 mDeadzone    = 0.08f;
			NkF32 mAxisEpsilon = 0.001f;
			std::unique_ptr<NkIGamepadMappingPersistence> mMappingPersistence;

			// Sentinelles pour les accÃ¨s invalides
			static NkGamepadSnapshot sDummySnapshot;
			static NkGamepadInfo     sDummyInfo;
	};

	// ---------------------------------------------------------------------------
	// Raccourci global
	// ---------------------------------------------------------------------------

	// NkGamepads() est dÃ©fini dans NkSystem.h pour Ã©viter la dÃ©pendance circulaire.
	// Il dÃ©lÃ¨gue Ã  NkSystem::Instance().GetGamepadSystem().
	// Inclure NkSystem.h (ou NkWindow.h) pour l'utiliser.

} // namespace nkentseu
