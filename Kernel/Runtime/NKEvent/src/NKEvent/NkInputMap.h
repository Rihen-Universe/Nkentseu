//
// NkInputMap.h
// =============================================================================
// Description :
//   LE systeme d'entree configurable du noyau : un jeu ou une application
//   declare des ACTIONS typees (bouton, axe 1D, axe 2D), les relie a des
//   ENTREES dans des CONTEXTES empilables, et lit ensuite des actions --
//   jamais des touches. Unkeny et Noge le partagent ; il ne connait ni l'un
//   ni l'autre.
//
// Caracteristiques :
//   - Actions typees : NK_INPUT_BUTTON (0..1), NK_INPUT_AXIS1D (-1..1),
//     NK_INPUT_AXIS2D (vecteur, longueur bornee a 1 : la diagonale au clavier
//     n'est pas 41 % plus rapide).
//   - Sources : un NkInputCode de NKEvent (touche, bouton de souris, molette,
//     bouton et axe de manette -- MEME texte que NkLoadBindings), un composite
//     (ZQSD/WASD -> axe 2D, deux touches -> axe 1D), un stick entier, le
//     deplacement de la souris, une zone d'ecran, un joystick virtuel
//     flottant, un geste de NkGestureRecognizer.
//   - Modificateurs par liaison : zone morte axiale ou radiale (avec butee),
//     echelle, inversion, permutation X/Y, courbe de reponse, lissage.
//   - Declencheurs par liaison : enfonce (defaut), presse, relache, maintenu
//     N s, tape, double tape ; ACCORD de touches (Ctrl+S), et l'accord bloque
//     la touche seule du meme contexte (Ctrl+S n'est pas aussi « S »).
//   - Contextes avec PRIORITE, allumes ou eteints ; un contexte qui CONSOMME
//     reserve ses entrees : le menu (priorite 10) prend Echap et les fleches,
//     le jeu (priorite 0) ne les voit plus tant que le menu est allume.
//   - Un NkInputMap = UN joueur : ses peripheriques lui sont ATTRIBUES
//     (clavier + souris, doigt, une manette, ou toutes). NkInputPlayers fait
//     rejoindre une manette au premier joueur qui n'en a pas.
//   - Capture « appuyez sur une touche » (BeginRebind) pour reconfigurer.
//   - Texte lisible, ligne par ligne, ligne fautive refusee AVEC son numero
//     (voir NkInputMapText.cpp pour le format).
//   - Lecture par INTERROGATION (IsDown, WasPressed, Value, Value2D) ET par
//     RAPPELS (OnAction : debut, en cours, fin).
//   - Aucune allocation par image : les tableaux vivent a la declaration.
//
// ⚠️ CE N'EST PAS NkActionManager / NkAxisManager, ET ILS RESTENT. Les deux
//    gestionnaires de NkEventDispatcher.h (par noms, par rappels, alimentes
//    automatiquement par NkWESystem) gardent leur API et leur conduite. Ce
//    systeme-ci est plus complet ; le texte de NkLoadBindings y reste lisible
//    pour les codes (« Key:SPACE »), pas pour les lignes (« action/axis »).
//
// Usage :
//     NkInputMap carte;
//     const NkInputActionId sauter = carte.DeclareAction("Sauter", NkInputValueType::NK_INPUT_BUTTON);
//     const NkInputActionId bouger = carte.DeclareAction("Bouger", NkInputValueType::NK_INPUT_AXIS2D);
//     const int32 jeu = carte.AddContext("Jeu", 0);
//     carte.Context(jeu)->Add(NkInputBinding::Key(sauter, NkKey::NK_SPACE));
//     carte.Context(jeu)->Add(NkInputBinding::Composite2D(bouger, NkInputCode::Key(NkKey::NK_D), ...));
//     // chaque evenement : carte.Read(ev) ; chaque image : carte.Update(dt, &manettes)
//     if (carte.WasPressed(sauter)) { ... }
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_EVENT_NKINPUTMAP_H__
#define __NKENTSEU_EVENT_NKINPUTMAP_H__

#include "NKContainers/Functional/NkFunction.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEvent/NkEvent.h"
#include "NKEvent/NkEventApi.h"
#include "NKEvent/NkEventDispatcher.h"
#include "NKEvent/NkGamepadSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKMath/NKMath.h"

namespace nkentseu {

	// =========================================================================
	// Les types
	// =========================================================================

	enum class NkInputValueType : uint8 {
		NK_INPUT_BUTTON = 0, ///< 0..1 : sauter, tirer
		NK_INPUT_AXIS1D,	 ///< -1..1 : avancer, tourner
		NK_INPUT_AXIS2D		 ///< vecteur de longueur <= 1 : se deplacer, viser
	};

	using NkInputActionId = int32;
	static constexpr NkInputActionId NK_INPUT_ACTION_INVALID = -1;

	/// Toutes les manettes branchees, pour un jeu a un seul joueur.
	static constexpr int32 NK_INPUT_ANY_GAMEPAD = -2;
	static constexpr int32 NK_INPUT_NO_GAMEPAD = -1;
	static constexpr int32 NK_INPUT_CHORD_MAX = 3;

	enum class NkInputSourceKind : uint8 {
		NK_INPUT_SRC_NONE = 0,
		NK_INPUT_SRC_CODE,			///< un NkInputCode (touche, souris, molette, bouton ou axe de manette)
		NK_INPUT_SRC_COMPOSITE,		///< 2 codes (+, -) -> 1D ; 4 codes (droite, gauche, haut, bas) -> 2D
		NK_INPUT_SRC_STICK,			///< un stick entier de la manette -> 2D (haut = +)
		NK_INPUT_SRC_MOUSE_DELTA,	///< deplacement de la souris cette image, en pixels (bas = +)
		NK_INPUT_SRC_SCREEN_BUTTON, ///< un doigt dans la zone -> 1
		NK_INPUT_SRC_SCREEN_STICK,	///< joystick virtuel flottant -> 2D (haut = +)
		NK_INPUT_SRC_GESTURE		///< un geste de NkGestureRecognizer
	};

	enum class NkInputGesture : uint8 {
		NK_INPUT_GESTURE_TAP = 0,	 ///< une impulsion par tape simple
		NK_INPUT_GESTURE_DOUBLE_TAP, ///< une impulsion par double tape
		NK_INPUT_GESTURE_LONG_PRESS,
		NK_INPUT_GESTURE_SWIPE_LEFT,
		NK_INPUT_GESTURE_SWIPE_RIGHT,
		NK_INPUT_GESTURE_SWIPE_UP,
		NK_INPUT_GESTURE_SWIPE_DOWN,
		NK_INPUT_GESTURE_PINCH,	 ///< 1D : somme des variations d'echelle de l'image (>0 = ecarter)
		NK_INPUT_GESTURE_ROTATE, ///< 1D : somme des variations d'angle de l'image, en degres
		NK_INPUT_GESTURE_PAN,	 ///< 2D : deplacement du centroide de l'image, en pixels
		NK_INPUT_GESTURE_COUNT
	};

	enum class NkInputDeadZone : uint8 {
		NK_INPUT_DEADZONE_NONE = 0,
		NK_INPUT_DEADZONE_AXIAL, ///< composante par composante
		NK_INPUT_DEADZONE_RADIAL ///< sur la longueur du vecteur -- la bonne pour un stick
	};

	enum class NkInputTriggerKind : uint8 {
		NK_INPUT_TRIGGER_DOWN = 0,	 ///< la valeur passe tant qu'elle existe (defaut)
		NK_INPUT_TRIGGER_PRESSED,	 ///< une image, a l'enfoncement
		NK_INPUT_TRIGGER_RELEASED,	 ///< une image, au relachement
		NK_INPUT_TRIGGER_HOLD,		 ///< passe une fois tenu `time` secondes
		NK_INPUT_TRIGGER_TAP,		 ///< une image, au relachement, si tenu moins de `time` s
		NK_INPUT_TRIGGER_DOUBLE_TAP	 ///< une image, au second enfoncement en moins de `time` s
	};

	enum class NkInputPhase : uint8 {
		NK_INPUT_STARTED = 0, ///< l'action vient de passer son seuil
		NK_INPUT_ONGOING,	  ///< elle le tient (chaque image)
		NK_INPUT_COMPLETED	  ///< elle vient de le perdre
	};

	/// Ce qu'une liaison fait a la valeur brute, dans cet ordre : zone morte,
	/// courbe, echelle, inversion, permutation, lissage.
	struct NkInputModifiers {
			NkInputDeadZone deadZone = NkInputDeadZone::NK_INPUT_DEADZONE_NONE;
			float32 deadZoneMin = 0.f; ///< en deca : zero
			float32 deadZoneMax = 1.f; ///< au-dela : course pleine (une manette usee n'atteint pas 1)
			float32 scaleX = 1.f;
			float32 scaleY = 1.f;
			bool invertX = false;
			bool invertY = false;
			bool swapXY = false;   ///< un axe 1D vers le Y d'une action 2D, ou l'inverse
			float32 curve = 1.f;   ///< exposant de la reponse : 2 = doux au centre
			float32 smoothing = 0.f; ///< constante de temps en secondes, 0 = instantane
	};

	struct NkInputTrigger {
			NkInputTriggerKind kind = NkInputTriggerKind::NK_INPUT_TRIGGER_DOWN;
			float32 time = 0.f; ///< HOLD / TAP / DOUBLE_TAP, en secondes
	};

	struct NkInputSource {
			NkInputSourceKind kind = NkInputSourceKind::NK_INPUT_SRC_NONE;
			NkInputCode code;
			/// Composite : 2 parties (plus, moins) ou 4 (droite, gauche, haut, bas).
			NkInputCode parts[4];
			uint8 partCount = 0;
			bool rightStick = false;
			NkInputGesture gesture = NkInputGesture::NK_INPUT_GESTURE_TAP;
			/// Zones et joystick : rectangle NORMALISE de la surface d'ecran.
			float32 zoneX = 0.f;
			float32 zoneY = 0.f;
			float32 zoneW = 0.f;
			float32 zoneH = 0.f;
			float32 radius = 0.08f; ///< joystick : course, en fraction de la HAUTEUR de la surface
	};

	/// Une liaison : UNE source vers UNE action, avec ses modificateurs, son
	/// declencheur et son accord. Les constructeurs statiques et les With*
	/// se composent : `NkInputBinding::Key(a, k).WithTrigger(...)`.
	struct NKENTSEU_EVENT_CLASS_EXPORT NkInputBinding {
			NkInputActionId action = NK_INPUT_ACTION_INVALID;
			NkInputSource source;
			NkInputModifiers modifiers;
			NkInputTrigger trigger;
			NkInputCode chord[NK_INPUT_CHORD_MAX];
			uint8 chordCount = 0;

			static NkInputBinding Code(NkInputActionId action, const NkInputCode &code) noexcept;
			static NkInputBinding Key(NkInputActionId action, NkKey key) noexcept;
			static NkInputBinding GamepadButton(NkInputActionId action, NkGamepadButton button) noexcept;
			static NkInputBinding GamepadAxis(NkInputActionId action, NkGamepadAxis axis) noexcept;
			static NkInputBinding Composite1D(NkInputActionId action, const NkInputCode &positive,
											  const NkInputCode &negative) noexcept;
			static NkInputBinding Composite2D(NkInputActionId action, const NkInputCode &right, const NkInputCode &left,
											  const NkInputCode &up, const NkInputCode &down) noexcept;
			static NkInputBinding Stick(NkInputActionId action, bool right = false) noexcept;
			static NkInputBinding MouseDelta(NkInputActionId action) noexcept;
			static NkInputBinding Gesture(NkInputActionId action, NkInputGesture gesture) noexcept;
			static NkInputBinding ScreenButton(NkInputActionId action, float32 x, float32 y, float32 w,
											   float32 h) noexcept;
			static NkInputBinding ScreenStick(NkInputActionId action, float32 x, float32 y, float32 w, float32 h,
											  float32 radius = 0.08f) noexcept;

			NkInputBinding WithDeadZone(NkInputDeadZone kind, float32 minimum, float32 maximum = 1.f) const noexcept;
			NkInputBinding WithScale(float32 x, float32 y = 1.f) const noexcept;
			NkInputBinding Inverted(bool x, bool y = false) const noexcept;
			NkInputBinding SwappedXY() const noexcept;
			NkInputBinding WithCurve(float32 exponent) const noexcept;
			NkInputBinding WithSmoothing(float32 seconds) const noexcept;
			NkInputBinding WithTrigger(NkInputTriggerKind kind, float32 time = 0.f) const noexcept;
			/// Ajoute une touche a tenir EN MEME TEMPS (Ctrl de Ctrl+S). 3 au plus.
			NkInputBinding WithChord(const NkInputCode &code) const noexcept;
	};

	/// Un ensemble de liaisons allume ou eteint d'un bloc : « Jeu », « Menu »,
	/// « Vehicule ». Plus sa PRIORITE est haute, plus tot il est lu.
	struct NKENTSEU_EVENT_CLASS_EXPORT NkInputContext {
			NkString name;
			int32 priority = 0;
			/// true : les entrees liees ici sont RESERVEES aux contextes de
			/// priorite superieure ou egale, tant que celui-ci est allume.
			bool consume = true;
			bool enabled = true;
			NkVector<NkInputBinding> bindings;

			/// Rend l'indice de la liaison.
			int32 Add(const NkInputBinding &binding);
			bool Remove(int32 index) noexcept;
			int32 Count() const noexcept {
				return static_cast<int32>(bindings.Size());
			}
	};

	/// L'etat d'une action, tel qu'on l'interroge.
	struct NkInputActionState {
			float32 x = 0.f;
			float32 y = 0.f;
			float32 prevX = 0.f;
			float32 prevY = 0.f;
			float32 threshold = 0.5f; ///< longueur au-dela de laquelle l'action est « enfoncee »
			float32 heldTime = 0.f;	  ///< secondes depuis qu'elle l'est
			bool down = false;
			bool pressed = false;  ///< cette image
			bool released = false; ///< cette image
	};

	/// Ce qu'une lecture de texte a fait, et ce qu'elle a refuse.
	struct NkInputMapReport {
			uint32 applied = 0;
			NkVector<NkString> errors; ///< une par ligne refusee, avec son numero

			bool Ok() const noexcept {
				return errors.Empty();
			}
	};

	using NkInputCallback = NkFunction<void(NkInputActionId, NkInputPhase, const NkInputActionState &)>;

	// =========================================================================
	// NkInputMap : UN joueur
	// =========================================================================
	class NKENTSEU_EVENT_CLASS_EXPORT NkInputMap {
		public:
			NkInputMap();

			// --- Actions ---------------------------------------------------------
			/// Declare une action ; si le nom existe, rend l'existante (et ne change
			/// pas son type). @param threshold longueur ou l'action devient enfoncee.
			NkInputActionId DeclareAction(const char *name, NkInputValueType type, float32 threshold = 0.5f);
			NkInputActionId FindAction(const char *name) const noexcept;
			int32 ActionCount() const noexcept;
			const char *ActionName(NkInputActionId id) const noexcept;
			NkInputValueType ActionType(NkInputActionId id) const noexcept;
			/// Oublie actions, contextes, liaisons et rappels.
			void Clear();

			// --- Contextes -------------------------------------------------------
			/// Rend l'indice du contexte ; s'il existe, rend l'existant.
			int32 AddContext(const char *name, int32 priority = 0, bool consume = true);
			int32 FindContext(const char *name) const noexcept;
			int32 ContextCount() const noexcept;
			/// ⚠️ LE POINTEUR VIT jusqu'au prochain AddContext (le tableau peut grandir).
			NkInputContext *Context(int32 index) noexcept;
			const NkInputContext *Context(int32 index) const noexcept;
			/// Allume ou eteint un contexte (Push / Pop d'un menu). Eteindre un
			/// contexte relache ce que ses liaisons tenaient.
			bool SetContextEnabled(int32 index, bool enabled) noexcept;
			bool SetContextEnabled(const char *name, bool enabled) noexcept;
			/// A appeler apres avoir modifie les liaisons d'un contexte a la main :
			/// l'etat de chaque liaison (temps tenu, lissage) repart de zero.
			void BindingsChanged() noexcept;

			// --- Peripheriques de CE joueur -----------------------------------------
			void SetGamepad(int32 index) noexcept; ///< 0..7, NK_INPUT_NO_GAMEPAD, NK_INPUT_ANY_GAMEPAD
			int32 GetGamepad() const noexcept {
				return mGamepad;
			}
			void SetAcceptKeyboardMouse(bool accept) noexcept;
			bool AcceptsKeyboardMouse() const noexcept {
				return mAcceptKeyboardMouse;
			}
			void SetAcceptTouch(bool accept) noexcept;
			bool AcceptsTouch() const noexcept {
				return mAcceptTouch;
			}
			/// La surface des zones d'ecran, en PIXELS CLIENT. Sans surface, zones
			/// et joysticks sont inertes.
			void SetScreenSurface(float32 x, float32 y, float32 w, float32 h) noexcept;

			// --- Alimentation ------------------------------------------------------
			/// Lit un evenement (clavier, souris, doigts, gestes). Rend true si une
			/// liaison d'un contexte allume vise cette entree (l'appelant peut la
			/// considerer comme prise par le jeu).
			bool Read(const NkEvent &event) noexcept;
			/// Une image : calcule toutes les actions, puis appelle les rappels.
			/// `pads` peut etre nul. ⚠️ UNE FOIS PAR IMAGE, avant la logique.
			void Update(float32 dt, const NkGamepadSystem *pads) noexcept;
			/// Relache tout (le jeu perd la main) : chaque action enfoncee passe
			/// en « relachee » TOUT DE SUITE, rappel COMPLETED compris.
			void ReleaseAll() noexcept;

			// --- Lecture par interrogation ------------------------------------------
			float32 Value(NkInputActionId id) const noexcept;
			math::NkVec2f Value2D(NkInputActionId id) const noexcept;
			bool IsDown(NkInputActionId id) const noexcept;
			bool WasPressed(NkInputActionId id) const noexcept;
			bool WasReleased(NkInputActionId id) const noexcept;
			float32 HeldTime(NkInputActionId id) const noexcept;
			const NkInputActionState &State(NkInputActionId id) const noexcept;
			/// L'action avait-elle au moins une liaison dans un contexte ALLUME au
			/// dernier Update ? (Sinon, sa valeur est celle posee par programme.)
			bool IsBound(NkInputActionId id) const noexcept;
			/// Pose une valeur PAR PROGRAMME (banc, script, IA). Si l'action a une
			/// liaison allumee, le prochain Update la recalcule depuis elle ; sinon
			/// la valeur posee RESTE (jusqu'a ce qu'une liaison s'allume). Une action
			/// sans liaison et sans valeur posee retombe a zero.
			void SetValue(NkInputActionId id, float32 x, float32 y = 0.f) noexcept;

			// --- Lecture par rappels -------------------------------------------------
			/// Rend un jeton (> 0) pour RemoveCallback.
			uint32 OnAction(NkInputActionId id, NkInputPhase phase, NkInputCallback callback);
			void RemoveCallback(uint32 token) noexcept;

			// --- Reconfiguration : « appuyez sur une touche » -----------------------
			/// La prochaine touche, le prochain bouton (souris, manette) ou le
			/// prochain axe pousse au-dela de la moitie remplace la source de la
			/// liaison (ou la PARTIE `part` d'un composite). Echap annule si
			/// `escapeCancels`. false si la liaison n'a pas de code a remplacer.
			bool BeginRebind(int32 context, int32 binding, int32 part = -1, bool escapeCancels = true) noexcept;
			bool IsRebinding() const noexcept {
				return mRebind.active;
			}
			void CancelRebind() noexcept;
			/// Vrai si la derniere capture a abouti (et pas annulee).
			bool LastRebindSucceeded() const noexcept {
				return mRebind.succeeded;
			}
			const NkInputCode &LastRebindCode() const noexcept {
				return mRebind.result;
			}

			// --- Texte ---------------------------------------------------------------
			/// Ecrit actions, contextes et liaisons (NkInputMapText.cpp).
			NkString Save() const;
			/// Relit un texte ecrit par Save ou a la main. `clearBindings` : les
			/// liaisons des contextes decrits sont remplacees (les actions et les
			/// contextes existants restent, avec leurs indices).
			NkInputMapReport Load(const NkString &text, bool clearBindings = true);

			// --- Diagnostic ------------------------------------------------------------
			bool IsKeyHeld(NkKey key) const noexcept;
			uint32 TouchCount() const noexcept;

		private:
			struct NkActionDef {
					char name[32] = {};
					NkInputValueType type = NkInputValueType::NK_INPUT_BUTTON;
			};
			struct NkBindingState {
					bool wasActive = false;
					float32 heldTime = 0.f;
					float32 sincePress = 1.0e9f;
					float32 smoothX = 0.f;
					float32 smoothY = 0.f;
					float32 lastX = 0.f;
					float32 lastY = 0.f;
			};
			struct NkFinger {
					uint64 id = 0;
					bool active = false;
					float32 u = 0.f;
					float32 v = 0.f;
					float32 u0 = 0.f;
					float32 v0 = 0.f;
					int32 stickContext = -1; ///< joystick qui l'a capture (contexte, liaison)
					int32 stickBinding = -1;
			};
			struct NkCallbackEntry {
					uint32 token = 0;
					NkInputActionId action = NK_INPUT_ACTION_INVALID;
					NkInputPhase phase = NkInputPhase::NK_INPUT_STARTED;
					NkInputCallback callback;
			};
			struct NkRebindState {
					bool active = false;
					bool succeeded = false;
					bool escapeCancels = true;
					int32 context = -1;
					int32 binding = -1;
					int32 part = -1;
					NkInputCode result;
			};
			/// Une entree reservee par un contexte qui consomme (cle interne).
			struct NkReservedKey {
					uint8 kind = 0;
					uint32 code = 0;
			};

			math::NkVec2f EvaluateSource(const NkInputBinding &b, int32 ctx, int32 index,
										 const NkGamepadSystem *pads) const noexcept;
			float32 CodeValue(const NkInputCode &code, const NkGamepadSystem *pads) const noexcept;
			float32 PadAxis(NkGamepadAxis axis, const NkGamepadSystem *pads) const noexcept;
			bool PadButton(NkGamepadButton button, const NkGamepadSystem *pads) const noexcept;
			bool ChordHeld(const NkInputBinding &b, const NkGamepadSystem *pads) const noexcept;
			static math::NkVec2f ApplyModifiers(const NkInputModifiers &m, math::NkVec2f v, bool radialPair,
												 NkBindingState &state, float32 dt) noexcept;
			bool IsReserved(const NkInputBinding &b) const noexcept;
			void Reserve(const NkInputBinding &b) noexcept;
			void ReserveKey(uint8 kind, uint32 code) noexcept;
			bool KeyReserved(uint8 kind, uint32 code) const noexcept;
			void Normalize(float32 x, float32 y, float32 &u, float32 &v) const noexcept;
			void CaptureFinger(NkFinger &f) noexcept;
			void FireCallbacks() noexcept;
			void FinishRebind(const NkInputCode &code) noexcept;
			void RebindFromPads(const NkGamepadSystem *pads) noexcept;
			void ResetBindingStates() noexcept;
			bool AnyBindingUses(const NkInputCode &code) const noexcept;
			bool AnyGestureBinding(NkInputGesture gesture) const noexcept;
			bool FingerStick(const NkFinger &f, const NkInputSource *&source) const noexcept;
			void SortedContexts(int32 *order, int32 &count) const noexcept;

			NkVector<NkActionDef> mDefs;
			NkVector<NkInputActionState> mStates;
			NkVector<math::NkVec2f> mSums;
			NkVector<uint8> mBound; ///< l'action a-t-elle une liaison allumee cette image ?
			NkVector<uint8> mManual; ///< valeur posee par SetValue, gardee tant qu'aucune liaison ne s'allume
			NkVector<NkInputContext> mContexts;
			NkVector<NkVector<NkBindingState>> mBindingStates;
			NkVector<NkCallbackEntry> mCallbacks;
			uint32 mNextToken = 1;

			// Peripheriques
			int32 mGamepad = NK_INPUT_ANY_GAMEPAD;
			bool mAcceptKeyboardMouse = true;
			bool mAcceptTouch = true;
			bool mKeys[static_cast<uint32>(NkKey::NK_KEY_MAX)] = {};
			bool mMouse[static_cast<uint32>(NkMouseButton::NK_MOUSE_BUTTON_MAX)] = {};
			float32 mWheelV = 0.f;
			float32 mWheelH = 0.f;
			float32 mMouseDX = 0.f;
			float32 mMouseDY = 0.f;
			NkFinger mFingers[NK_MAX_TOUCH_POINTS];
			float32 mSurfX = 0.f;
			float32 mSurfY = 0.f;
			float32 mSurfW = 0.f;
			float32 mSurfH = 0.f;
			float32 mGestureImpulse[static_cast<uint32>(NkInputGesture::NK_INPUT_GESTURE_COUNT)] = {};
			float32 mPanX = 0.f;
			float32 mPanY = 0.f;

			// Reservations de l'image (contextes qui consomment) et blocages d'accord
			NkReservedKey mReserved[256];
			int32 mReservedCount = 0;
			NkInputCode mChordBlocked[32];
			int32 mChordBlockedCount = 0;

			NkRebindState mRebind;
			bool mPadButtonsBefore[NK_MAX_GAMEPADS][static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX)] = {};
	};

	// =========================================================================
	// NkInputPlayers : plusieurs joueurs, et qui a quelle manette
	// =========================================================================
	/// Garde des cartes (non possedees) et fait REJOINDRE une manette : la
	/// premiere manette non attribuee dont un bouton (Start, Sud...) vient
	/// d'etre enfonce est donnee au premier joueur qui n'en a pas.
	class NKENTSEU_EVENT_CLASS_EXPORT NkInputPlayers {
		public:
			static constexpr int32 MAX_PLAYERS = 4;

			/// Rend l'indice du joueur, ou -1 si plein.
			int32 Add(NkInputMap &map) noexcept;
			int32 Count() const noexcept {
				return mCount;
			}
			NkInputMap *Player(int32 index) noexcept;
			/// Le joueur qui tient cette manette, ou -1.
			int32 PlayerForGamepad(int32 pad) const noexcept;
			/// Detecte les manettes qui rejoignent. Rend le joueur qui vient d'en
			/// recevoir une, ou -1. `joinButton` : NK_GP_UNKNOWN = n'importe lequel.
			int32 Update(const NkGamepadSystem *pads,
						 NkGamepadButton joinButton = NkGamepadButton::NK_GP_UNKNOWN) noexcept;

		private:
			NkInputMap *mMaps[MAX_PLAYERS] = {};
			int32 mCount = 0;
			bool mBefore[NK_MAX_GAMEPADS][static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX)] = {};
	};

} // namespace nkentseu

#endif // __NKENTSEU_EVENT_NKINPUTMAP_H__
