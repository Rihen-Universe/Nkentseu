#pragma once
// =============================================================================
// Nkentseu/Design/Color/NkColorManager.h
// =============================================================================
// Gestion colorimétrique complète pour les applications de design.
//
// ESPACES COLORIMÉTIQUES :
//   sRGB       — Standard web/écran (non-linéaire, gamma 2.2)
//   Linear RGB — GPU interne (linéaire, 0..1)
//   HSL        — Teinte/Saturation/Luminosité (perceptuel)
//   HSV        — Teinte/Saturation/Valeur (pipette intuitive)
//   LAB        — L*a*b* CIELAB (perceptuellement uniforme)
//   LCH        — L*C*H° (Lab en coordonnées cylindriques)
//   CMYK       — Impression (Cyan/Magenta/Yellow/Key)
//   Hex        — #RRGGBB ou #RRGGBBAA (web)
//
// CONVERSIONS :
//   Le type couleur de ce module est `nkentseu::math::NkColorF` (NKMath/NkColor.h)
//   -- pas de classe NkColor locale (fusionnée dans NKMath le 2026-07-25, voir
//   commentaire [FUSION 2026-07-25] plus bas). Toutes les conversions passent
//   par sRGB linéarisé comme espace pivot : NkColorF::ToLinearRGB() /
//   NkColorF::FromLinearRGB() pour chaque espace.
//
// PALETTES :
//   NkPalette  — collection de couleurs nommées
//   NkSwatch   — couleur rapide avec nom et étiquette
//   NkHarmony  — générateur d'harmonies colorimétiques
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	// [FUSION 2026-07-25] Ce fichier définissait sa PROPRE `class NkColor`
	// (dans le namespace `nkentseu`), en doublon de `nkentseu::math::NkColor`
	// (NKMath/NkColor.h). Le doublon rendait `NkColor` ambigu dès qu'un
	// `using namespace math;` était actif dans une unité de compilation qui
	// incluait aussi ce fichier -- contourné en 2026-07-24 par des `using`
	// ciblés (voir historique git). La cause racine est corrigée ici : la
	// classe dupliquée est supprimée, ses capacités déclarées (mais jamais
	// implémentées -- 0 % de corps de fonction hors FromSRGB/SRGBToLinear/
	// LinearToSRGB, cf. NkColorManager.cpp historique) ont été fusionnées et
	// RÉELLEMENT implémentées dans `math::NkColorF` (NKMath/NkColor.h/.cpp) :
	// FromHSL/FromLAB/FromLCH/FromCMYK/FromOKLab/FromOKLch/FromHex/
	// FromLinearRGB, ToHSL/ToLAB/ToLCH/ToCMYK/ToOKLab/ToOKLch/ToHexString/
	// ToLinearRGB/ToXYZ/FromXYZ, ajustements (With*/Saturate/Desaturate/
	// Complement/Invert/Brighten), métriques (Luminance/DeltaE/DeltaE2000/
	// ContrastRatio/IsAccessible/BestContrast), modes de fusion (Multiply/
	// Screen/Overlay/SoftLight/HardLight), interpolation (Lerp/LerpLAB/
	// LerpOKLab/Mix). `NkColorSpace`/`NkSwatch`/`NkPalette`/`NkHarmony`/
	// `NkColorPicker`/`NkColorManager` sont conservés ici, adaptés pour
	// utiliser `math::NkColorF` au lieu de l'ancien type dupliqué.
	using math::NkVec4f;
	using math::NkClamp;
	using math::NkColorF;

	// =========================================================================
	// NkColorSpace — espace colorimétrique
	// =========================================================================
	enum class NkColorSpace : uint8 { LinearRGB, sRGB, HSL, HSV, LAB, LCH, CMYK, XYZ, OKLab, OKLch };

	// =========================================================================
	// NkSwatch — couleur avec nom et étiquette
	// =========================================================================
	struct NkSwatch {
			static constexpr uint32 kMaxName = 64u;
			char name[kMaxName] = {};
			NkColorF color;
			bool selected = false;

			NkSwatch() noexcept = default;

			NkSwatch(const char *n, const NkColorF &c) noexcept : color(c) {
				std::strncpy(name, n, kMaxName - 1);
			}
	};

	// =========================================================================
	// NkPalette — collection de nuanciers
	// =========================================================================
	class NkPalette {
		public:
			NkString name;
			NkVector<NkSwatch> swatches;

			NkPalette() noexcept = default;

			explicit NkPalette(const char *n) noexcept : name(n) {
			}

			void Add(const char *name, const NkColorF &color) noexcept {
				swatches.PushBack(NkSwatch(name, color));
			}

			[[nodiscard]] const NkSwatch *Find(const char *name) const noexcept;
			[[nodiscard]] NkColorF Get(uint32 idx) const noexcept;

			[[nodiscard]] uint32 Count() const noexcept {
				return static_cast<uint32>(swatches.Size());
			}

			// Palettes prédéfinies
			[[nodiscard]] static NkPalette Material() noexcept; ///< Material Design 3
			[[nodiscard]] static NkPalette Tailwind() noexcept; ///< Tailwind CSS
			[[nodiscard]] static NkPalette Pastels() noexcept;
			[[nodiscard]] static NkPalette Monochrome() noexcept;
			[[nodiscard]] static NkPalette WebSafe() noexcept;

			// Import/Export
			bool SaveToFile(const char *path) const noexcept; ///< .ase, .aco, .json
			bool LoadFromFile(const char *path) noexcept;
	};

	// =========================================================================
	// NkHarmony — générateur d'harmonies colorimétrique
	// =========================================================================
	class NkHarmony {
		public:
			/**
			 * @brief Génère les couleurs complémentaires (180°).
			 */
			[[nodiscard]] static NkVector<NkColorF> Complementary(const NkColorF &base) noexcept;

			/**
			 * @brief Triade (3 couleurs espacées de 120°).
			 */
			[[nodiscard]] static NkVector<NkColorF> Triadic(const NkColorF &base) noexcept;

			/**
			 * @brief Tétrade (4 couleurs espacées de 90°).
			 */
			[[nodiscard]] static NkVector<NkColorF> Tetradic(const NkColorF &base) noexcept;

			/**
			 * @brief Analogues (3 couleurs adjacentes ±30°).
			 */
			[[nodiscard]] static NkVector<NkColorF> Analogous(const NkColorF &base, float32 angle = 30.f) noexcept;

			/**
			 * @brief Split-complémentaires.
			 */
			[[nodiscard]] static NkVector<NkColorF> SplitComplementary(const NkColorF &base,
																	   float32 split = 30.f) noexcept;

			/**
			 * @brief Génère N nuances monochromatiques (same hue, varying L).
			 */
			[[nodiscard]] static NkVector<NkColorF> Monochromatic(const NkColorF &base, uint32 count = 5) noexcept;

			/**
			 * @brief Gradient perceptuellement uniforme entre deux couleurs (via OKLab).
			 */
			[[nodiscard]] static NkVector<NkColorF> GradientOKLab(const NkColorF &from, const NkColorF &to,
																  uint32 steps = 5) noexcept;
	};

	// =========================================================================
	// NkColorPicker — état du color picker UI
	// =========================================================================
	struct NkColorPicker {
			NkColorF currentColor;
			NkColorF previousColor;
			NkColorSpace displaySpace = NkColorSpace::HSV;

			// Valeurs dans l'espace d'affichage
			float32 h = 0.f, s = 1.f, v = 1.f; ///< HSV
			float32 r = 0.f, g = 0.f, b = 0.f; ///< Linear RGB
			float32 a = 1.f;
			NkString hexStr;

			void SetColor(const NkColorF &c) noexcept;
			void SyncFromHSV() noexcept;
			void SyncFromRGB() noexcept;
			void SyncFromHex() noexcept;
	};

	// =========================================================================
	// NkColorManager — gestionnaire global de couleurs (singleton)
	// =========================================================================
	class NkColorManager {
		public:
			[[nodiscard]] static NkColorManager &Global() noexcept {
				static NkColorManager inst;
				return inst;
			}

			// ── Couleurs actives ──────────────────────────────────────────────
			NkColorF foreground = math::NkColor::Black.ToColorF();
			NkColorF background = math::NkColor::White.ToColorF();

			void SwapFgBg() noexcept {
				std::swap(foreground, background);
			}

			void ResetDefault() noexcept {
				foreground = math::NkColor::Black.ToColorF();
				background = math::NkColor::White.ToColorF();
			}

			// ── Palettes ──────────────────────────────────────────────────────
			NkVector<NkPalette> palettes;

			NkPalette &AddPalette(const char *name) noexcept {
				palettes.PushBack(NkPalette(name));
				return palettes.Back();
			}

			// ── Historique des couleurs récentes ─────────────────────────────
			static constexpr uint32 kHistorySize = 32u;
			NkColorF history[kHistorySize] = {};
			uint32 historyCount = 0;

			void PushHistory(const NkColorF &c) noexcept;

			// ── Pipette (screen color sampling) ──────────────────────────────
			/**
			 * @brief Échantillonne la couleur à une position écran.
			 * Lit le pixel depuis le framebuffer via NKRHI readback.
			 */
			[[nodiscard]] NkColorF SampleScreen(uint32 x, uint32 y) const noexcept;

		private:
			NkColorManager() noexcept = default;
	};

} // namespace nkentseu
