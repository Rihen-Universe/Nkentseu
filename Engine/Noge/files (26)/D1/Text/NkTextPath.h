
#pragma once
// =============================================================================
// Nkentseu/Design/Text/NkTextPath.h — Texte sur chemin + typographie avancée
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "Nkentseu/Design/Vector/NkVectorPath.h"
#include "Nkentseu/Design/Color/NkColorManager.h"

namespace nkentseu {
using namespace math;

enum class NkTextAlign : uint8 { Left, Center, Right, Justify };
enum class NkVerticalAlign : uint8 { Top, Middle, Bottom };
enum class NkTextDecoration : uint8 { None, Underline, Strikethrough, Overline };

struct NkFontStyle {
    NkString    family      = "Arial";
    float32     size        = 14.f;     // pts
    bool        bold        = false;
    bool        italic      = false;
    NkTextDecoration decoration = NkTextDecoration::None;
    float32     letterSpacing  = 0.f;  // em
    float32     lineHeight     = 1.2f; // em
    NkColor     color       = NkColor::Black();
};

struct NkTextRun { NkString text; NkFontStyle style; };

// Texte multi-style (différents styles dans le même paragraphe)
class NkRichText {
public:
    NkVector<NkTextRun> runs;
    NkTextAlign   align   = NkTextAlign::Left;
    NkVerticalAlign vAlign = NkVerticalAlign::Top;
    float32 width = 0.f;  // 0 = pas de retour à la ligne

    void AddRun(const char* text, const NkFontStyle& style) noexcept;
    void Draw(renderer::NkRender2D& r2d, NkVec2f pos) const noexcept;
    [[nodiscard]] NkVec2f MeasureSize() const noexcept;
};

// Texte le long d'un chemin vectoriel
class NkTextOnPath {
public:
    NkVectorPath path;
    NkRichText   text;
    float32      startOffset = 0.f;  // [0..1] ou pixels selon unité
    bool         side        = false; // false=dessus, true=dessous

    void Draw(renderer::NkRender2D& r2d) const noexcept;
};
}
