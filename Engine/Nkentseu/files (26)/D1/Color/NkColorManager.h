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
//   Toutes les conversions passent par Linear RGB comme espace pivot.
//   NkColor::ToLinear() / NkColor::FromLinear() pour chaque espace.
//
// PALETTES :
//   NkPalette  — collection de couleurs nommées
//   NkSwatch   — couleur rapide avec nom et étiquette
//   NkHarmony  — générateur d'harmonies colorimétiques
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    using namespace math;

    // =========================================================================
    // NkColorSpace — espace colorimétrique
    // =========================================================================
    enum class NkColorSpace : uint8 {
        LinearRGB, sRGB, HSL, HSV, LAB, LCH, CMYK, XYZ, OKLab, OKLch
    };

    // =========================================================================
    // NkColor — couleur universelle avec conversion entre espaces
    // =========================================================================
    /**
     * @class NkColor
     * @brief Représentation universelle d'une couleur avec conversion d'espace.
     *
     * En interne, stockée en Linear RGB [0..1] + alpha [0..1].
     * Toutes les conversions sont exactes et réversibles.
     */
    class NkColor {
    public:
        // ── Constructeurs ─────────────────────────────────────────────────
        NkColor() noexcept = default;
        NkColor(float32 r, float32 g, float32 b, float32 a = 1.f) noexcept
            : mR(r), mG(g), mB(b), mA(a) {}

        // ── Factory par espace ────────────────────────────────────────────
        [[nodiscard]] static NkColor FromSRGB   (float32 r,float32 g,float32 b,float32 a=1.f) noexcept;
        [[nodiscard]] static NkColor FromHSL    (float32 h,float32 s,float32 l,float32 a=1.f) noexcept;
        [[nodiscard]] static NkColor FromHSV    (float32 h,float32 s,float32 v,float32 a=1.f) noexcept;
        [[nodiscard]] static NkColor FromLAB    (float32 L,float32 a,float32 b,float32 alpha=1.f) noexcept;
        [[nodiscard]] static NkColor FromLCH    (float32 L,float32 C,float32 H,float32 a=1.f) noexcept;
        [[nodiscard]] static NkColor FromCMYK   (float32 c,float32 m,float32 y,float32 k,float32 a=1.f) noexcept;
        [[nodiscard]] static NkColor FromOKLab  (float32 L,float32 a,float32 b,float32 alpha=1.f) noexcept;
        [[nodiscard]] static NkColor FromHex    (const char* hex) noexcept;    // "#RRGGBB[AA]"
        [[nodiscard]] static NkColor FromU8     (uint8 r,uint8 g,uint8 b,uint8 a=255) noexcept;

        // ── Conversion vers espaces ────────────────────────────────────────
        [[nodiscard]] NkVec4f ToLinearRGB() const noexcept { return {mR,mG,mB,mA}; }
        [[nodiscard]] NkVec4f ToSRGB()      const noexcept;
        [[nodiscard]] NkVec4f ToHSL()       const noexcept;
        [[nodiscard]] NkVec4f ToHSV()       const noexcept;
        [[nodiscard]] NkVec4f ToLAB()       const noexcept;
        [[nodiscard]] NkVec4f ToLCH()       const noexcept;
        [[nodiscard]] NkVec4f ToCMYK()      const noexcept;
        [[nodiscard]] NkVec4f ToOKLab()     const noexcept;
        [[nodiscard]] NkString ToHexString(bool withAlpha = false) const noexcept;
        [[nodiscard]] NkVec4f ToU8()        const noexcept;  ///< [0..255]

        // ── Accès direct ─────────────────────────────────────────────────
        [[nodiscard]] float32 R() const noexcept { return mR; }
        [[nodiscard]] float32 G() const noexcept { return mG; }
        [[nodiscard]] float32 B() const noexcept { return mB; }
        [[nodiscard]] float32 A() const noexcept { return mA; }
        void SetAlpha(float32 a) noexcept { mA = NkClamp(a, 0.f, 1.f); }

        // ── Ajustements ───────────────────────────────────────────────────
        [[nodiscard]] NkColor WithHue       (float32 h) const noexcept;
        [[nodiscard]] NkColor WithSaturation(float32 s) const noexcept;
        [[nodiscard]] NkColor WithLightness (float32 l) const noexcept;
        [[nodiscard]] NkColor WithValue     (float32 v) const noexcept;
        [[nodiscard]] NkColor WithAlpha     (float32 a) const noexcept;
        [[nodiscard]] NkColor Brighten      (float32 amount) const noexcept;
        [[nodiscard]] NkColor Darken        (float32 amount)  const noexcept;
        [[nodiscard]] NkColor Saturate      (float32 amount)  const noexcept;
        [[nodiscard]] NkColor Desaturate    (float32 amount)  const noexcept;
        [[nodiscard]] NkColor Complement    () const noexcept;
        [[nodiscard]] NkColor Invert        () const noexcept;

        // ── Métriques perceptuelles ───────────────────────────────────────
        [[nodiscard]] float32 Luminance()         const noexcept;  ///< [0..1] relatif
        [[nodiscard]] float32 DeltaE(const NkColor& other) const noexcept;  ///< CIE76
        [[nodiscard]] float32 DeltaE2000(const NkColor& other) const noexcept;  ///< CIEDE2000
        [[nodiscard]] float32 ContrastRatio(const NkColor& other) const noexcept;
        [[nodiscard]] bool    IsAccessible(const NkColor& bg, float32 minRatio=4.5f) const noexcept;
        [[nodiscard]] NkColor BestContrast(const NkColor& dark, const NkColor& light) const noexcept;

        // ── Opérations ────────────────────────────────────────────────────
        [[nodiscard]] static NkColor Lerp     (const NkColor& a, const NkColor& b, float32 t) noexcept;
        [[nodiscard]] static NkColor LerpLAB  (const NkColor& a, const NkColor& b, float32 t) noexcept;
        [[nodiscard]] static NkColor LerpOKLab(const NkColor& a, const NkColor& b, float32 t) noexcept;
        [[nodiscard]] static NkColor Mix      (const NkColor& a, const NkColor& b, float32 t) noexcept;

        // Blend modes (résultat dans Linear RGB)
        [[nodiscard]] NkColor Multiply  (const NkColor& other) const noexcept;
        [[nodiscard]] NkColor Screen    (const NkColor& other) const noexcept;
        [[nodiscard]] NkColor Overlay   (const NkColor& other) const noexcept;
        [[nodiscard]] NkColor SoftLight (const NkColor& other) const noexcept;
        [[nodiscard]] NkColor HardLight (const NkColor& other) const noexcept;

        // ── Couleurs nommées ──────────────────────────────────────────────
        [[nodiscard]] static NkColor Black()       noexcept { return {0,0,0,1}; }
        [[nodiscard]] static NkColor White()       noexcept { return {1,1,1,1}; }
        [[nodiscard]] static NkColor Transparent() noexcept { return {0,0,0,0}; }
        [[nodiscard]] static NkColor Red()         noexcept;
        [[nodiscard]] static NkColor Green()       noexcept;
        [[nodiscard]] static NkColor Blue()        noexcept;
        [[nodiscard]] static NkColor Yellow()      noexcept;
        [[nodiscard]] static NkColor Cyan()        noexcept;
        [[nodiscard]] static NkColor Magenta()     noexcept;

        bool operator==(const NkColor& o) const noexcept;
        bool operator!=(const NkColor& o) const noexcept;

    private:
        float32 mR = 0.f, mG = 0.f, mB = 0.f, mA = 1.f;  ///< Linear RGB

        [[nodiscard]] static float32 SRGBToLinear(float32 x) noexcept;
        [[nodiscard]] static float32 LinearToSRGB(float32 x) noexcept;
    };

    // =========================================================================
    // NkSwatch — couleur avec nom et étiquette
    // =========================================================================
    struct NkSwatch {
        static constexpr uint32 kMaxName = 64u;
        char    name[kMaxName] = {};
        NkColor color;
        bool    selected = false;

        NkSwatch() noexcept = default;
        NkSwatch(const char* n, const NkColor& c) noexcept : color(c) {
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
        explicit NkPalette(const char* n) noexcept : name(n) {}

        void Add(const char* name, const NkColor& color) noexcept {
            swatches.PushBack(NkSwatch(name, color));
        }

        [[nodiscard]] const NkSwatch* Find(const char* name) const noexcept;
        [[nodiscard]] NkColor         Get (uint32 idx) const noexcept;
        [[nodiscard]] uint32          Count() const noexcept { return static_cast<uint32>(swatches.Size()); }

        // Palettes prédéfinies
        [[nodiscard]] static NkPalette Material()     noexcept;  ///< Material Design 3
        [[nodiscard]] static NkPalette Tailwind()     noexcept;  ///< Tailwind CSS
        [[nodiscard]] static NkPalette Pastels()      noexcept;
        [[nodiscard]] static NkPalette Monochrome()   noexcept;
        [[nodiscard]] static NkPalette WebSafe()      noexcept;

        // Import/Export
        bool SaveToFile  (const char* path) const noexcept;  ///< .ase, .aco, .json
        bool LoadFromFile(const char* path) noexcept;
    };

    // =========================================================================
    // NkHarmony — générateur d'harmonies colorimétrique
    // =========================================================================
    class NkHarmony {
    public:
        /**
         * @brief Génère les couleurs complémentaires (180°).
         */
        [[nodiscard]] static NkVector<NkColor>
        Complementary(const NkColor& base) noexcept;

        /**
         * @brief Triade (3 couleurs espacées de 120°).
         */
        [[nodiscard]] static NkVector<NkColor>
        Triadic(const NkColor& base) noexcept;

        /**
         * @brief Tétrade (4 couleurs espacées de 90°).
         */
        [[nodiscard]] static NkVector<NkColor>
        Tetradic(const NkColor& base) noexcept;

        /**
         * @brief Analogues (3 couleurs adjacentes ±30°).
         */
        [[nodiscard]] static NkVector<NkColor>
        Analogous(const NkColor& base, float32 angle = 30.f) noexcept;

        /**
         * @brief Split-complémentaires.
         */
        [[nodiscard]] static NkVector<NkColor>
        SplitComplementary(const NkColor& base, float32 split = 30.f) noexcept;

        /**
         * @brief Génère N nuances monochromatiques (same hue, varying L).
         */
        [[nodiscard]] static NkVector<NkColor>
        Monochromatic(const NkColor& base, uint32 count = 5) noexcept;

        /**
         * @brief Gradient perceptuellement uniforme entre deux couleurs (via OKLab).
         */
        [[nodiscard]] static NkVector<NkColor>
        GradientOKLab(const NkColor& from, const NkColor& to,
                       uint32 steps = 5) noexcept;
    };

    // =========================================================================
    // NkColorPicker — état du color picker UI
    // =========================================================================
    struct NkColorPicker {
        NkColor   currentColor;
        NkColor   previousColor;
        NkColorSpace displaySpace = NkColorSpace::HSV;

        // Valeurs dans l'espace d'affichage
        float32   h = 0.f, s = 1.f, v = 1.f;  ///< HSV
        float32   r = 0.f, g = 0.f, b = 0.f;  ///< Linear RGB
        float32   a = 1.f;
        NkString  hexStr;

        void SetColor(const NkColor& c) noexcept;
        void SyncFromHSV() noexcept;
        void SyncFromRGB() noexcept;
        void SyncFromHex() noexcept;
    };

    // =========================================================================
    // NkColorManager — gestionnaire global de couleurs (singleton)
    // =========================================================================
    class NkColorManager {
    public:
        [[nodiscard]] static NkColorManager& Global() noexcept {
            static NkColorManager inst;
            return inst;
        }

        // ── Couleurs actives ──────────────────────────────────────────────
        NkColor foreground = NkColor::Black();
        NkColor background = NkColor::White();

        void SwapFgBg() noexcept { std::swap(foreground, background); }
        void ResetDefault() noexcept { foreground = NkColor::Black(); background = NkColor::White(); }

        // ── Palettes ──────────────────────────────────────────────────────
        NkVector<NkPalette> palettes;

        NkPalette& AddPalette(const char* name) noexcept {
            palettes.PushBack(NkPalette(name));
            return palettes.Back();
        }

        // ── Historique des couleurs récentes ─────────────────────────────
        static constexpr uint32 kHistorySize = 32u;
        NkColor history[kHistorySize] = {};
        uint32  historyCount = 0;

        void PushHistory(const NkColor& c) noexcept;

        // ── Pipette (screen color sampling) ──────────────────────────────
        /**
         * @brief Échantillonne la couleur à une position écran.
         * Lit le pixel depuis le framebuffer via NKRHI readback.
         */
        [[nodiscard]] NkColor SampleScreen(uint32 x, uint32 y) const noexcept;

    private:
        NkColorManager() noexcept = default;
    };

} // namespace nkentseu
