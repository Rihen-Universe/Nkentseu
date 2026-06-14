// =============================================================================
// NkSVGCodec.cpp
// -----------------------------------------------------------------------------
// Reecriture complete from scratch 2026-05-19, inspiree de NanoSVG par
// Mikko Mononen (sans inclure la lib). Single-file rasterizer SVG -> NkImage RGBA.
//
// Architecture :
//   1. Parser XML stream-based avec stack pour <g> (push/pop style + transform).
//   2. Chaque shape produit une Shape interne (path polyligne apres flatten,
//      style cumule, CTM cumule deja applique aux points).
//   3. Rasterizer scanline avec supersample 2x AA, fill-rule nonzero/evenodd,
//      alpha blending sur l'image RGBA32 destination.
//
// Features supportees :
//   - <svg> avec viewBox + width/height
//   - <g> avec transform cascade correcte (BUG fixe vs ancienne version)
//   - <path> M L H V C S Q T A Z (absolu/relatif) avec text-to-path 24+ KB OK
//   - <rect> <circle> <ellipse> <line> <polyline> <polygon>
//   - Cubic + quadratic Bezier flatten adaptatif
//   - Arc elliptique conversion W3C SVG 1.1 Appendix F.6
//   - fill, stroke (color), opacity, fill-opacity, fill-rule
//   - transform: translate, scale, rotate, matrix
//   - Couleurs : #RGB #RRGGBB rgb() rgba() + 148 noms CSS
//   - Attribut style="..." CSS inline
//
// Non implemente (Phase 2) :
//   - <text> <tspan> (necessite font system)
//   - <defs><style> avec class CSS
//   - Gradients, patterns, masks, clip-path, filters
//   - Stroke (uniquement fill rendu pour Phase 1)
//   - <use> <symbol> <image>
// =============================================================================

#include "NKImage/Codecs/SVG/NkSVGCodec.h"
#include "NKImage/Core/NkImage.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NkAllocator.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <new>

namespace nkentseu {

using namespace nkentseu::memory;

namespace {  // file-local helpers (anonymous namespace)

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 1 — Helpers parsing texte
// ─────────────────────────────────────────────────────────────────────────────

inline bool IsSpace(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
inline bool IsDigit(char c) noexcept { return c >= '0' && c <= '9'; }

inline const char* SkipWS(const char* p) noexcept
{
    while (*p && IsSpace(*p)) ++p;
    return p;
}
inline const char* SkipWSComma(const char* p) noexcept
{
    while (*p && (IsSpace(*p) || *p == ',')) ++p;
    return p;
}

/// Parse un float style strtod (sans locale). Avance @p end si fourni.
float32 ParseFloat(const char* s, const char** endOut = nullptr) noexcept
{
    const char* p = s ? SkipWS(s) : nullptr;
    if (!p) { if (endOut) *endOut = nullptr; return 0.f; }
    float32 sign = 1.f;
    if (*p == '+') ++p;
    else if (*p == '-') { sign = -1.f; ++p; }
    float64 v = 0.0;
    while (IsDigit(*p)) { v = v * 10.0 + (float64)(*p - '0'); ++p; }
    if (*p == '.') {
        ++p;
        float64 f = 0.1;
        while (IsDigit(*p)) { v += (float64)(*p - '0') * f; f *= 0.1; ++p; }
    }
    if (*p == 'e' || *p == 'E') {
        ++p;
        int32 esign = 1, e = 0;
        if (*p == '+') ++p;
        else if (*p == '-') { esign = -1; ++p; }
        while (IsDigit(*p)) { e = e * 10 + (*p - '0'); ++p; }
        v *= std::pow(10.0, (float64)(esign * e));
    }
    if (endOut) *endOut = p;
    return (float32)(sign * v);
}

/// Strcasecmp portable (les noms CSS sont insensibles a la casse).
int32 StrCaseCmp(const char* a, const char* b) noexcept
{
    while (*a && *b) {
        const char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        const char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (ca != cb) return ca - cb;
        ++a; ++b;
    }
    return *a - *b;
}

int32 HexDigit(char c) noexcept
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 2 — Table des noms de couleurs CSS (148 noms standards)
// ─────────────────────────────────────────────────────────────────────────────

struct NamedColor { const char* name; uint8 r, g, b; };

const NamedColor kNamedColors[] = {
    {"aliceblue",240,248,255},{"antiquewhite",250,235,215},{"aqua",0,255,255},
    {"aquamarine",127,255,212},{"azure",240,255,255},{"beige",245,245,220},
    {"bisque",255,228,196},{"black",0,0,0},{"blanchedalmond",255,235,205},
    {"blue",0,0,255},{"blueviolet",138,43,226},{"brown",165,42,42},
    {"burlywood",222,184,135},{"cadetblue",95,158,160},{"chartreuse",127,255,0},
    {"chocolate",210,105,30},{"coral",255,127,80},{"cornflowerblue",100,149,237},
    {"cornsilk",255,248,220},{"crimson",220,20,60},{"cyan",0,255,255},
    {"darkblue",0,0,139},{"darkcyan",0,139,139},{"darkgoldenrod",184,134,11},
    {"darkgray",169,169,169},{"darkgreen",0,100,0},{"darkgrey",169,169,169},
    {"darkkhaki",189,183,107},{"darkmagenta",139,0,139},{"darkolivegreen",85,107,47},
    {"darkorange",255,140,0},{"darkorchid",153,50,204},{"darkred",139,0,0},
    {"darksalmon",233,150,122},{"darkseagreen",143,188,143},{"darkslateblue",72,61,139},
    {"darkslategray",47,79,79},{"darkslategrey",47,79,79},{"darkturquoise",0,206,209},
    {"darkviolet",148,0,211},{"deeppink",255,20,147},{"deepskyblue",0,191,255},
    {"dimgray",105,105,105},{"dimgrey",105,105,105},{"dodgerblue",30,144,255},
    {"firebrick",178,34,34},{"floralwhite",255,250,240},{"forestgreen",34,139,34},
    {"fuchsia",255,0,255},{"gainsboro",220,220,220},{"ghostwhite",248,248,255},
    {"gold",255,215,0},{"goldenrod",218,165,32},{"gray",128,128,128},
    {"green",0,128,0},{"greenyellow",173,255,47},{"grey",128,128,128},
    {"honeydew",240,255,240},{"hotpink",255,105,180},{"indianred",205,92,92},
    {"indigo",75,0,130},{"ivory",255,255,240},{"khaki",240,230,140},
    {"lavender",230,230,250},{"lavenderblush",255,240,245},{"lawngreen",124,252,0},
    {"lemonchiffon",255,250,205},{"lightblue",173,216,230},{"lightcoral",240,128,128},
    {"lightcyan",224,255,255},{"lightgoldenrodyellow",250,250,210},
    {"lightgray",211,211,211},{"lightgreen",144,238,144},{"lightgrey",211,211,211},
    {"lightpink",255,182,193},{"lightsalmon",255,160,122},{"lightseagreen",32,178,170},
    {"lightskyblue",135,206,250},{"lightslategray",119,136,153},
    {"lightslategrey",119,136,153},{"lightsteelblue",176,196,222},
    {"lightyellow",255,255,224},{"lime",0,255,0},{"limegreen",50,205,50},
    {"linen",250,240,230},{"magenta",255,0,255},{"maroon",128,0,0},
    {"mediumaquamarine",102,205,170},{"mediumblue",0,0,205},
    {"mediumorchid",186,85,211},{"mediumpurple",147,112,219},
    {"mediumseagreen",60,179,113},{"mediumslateblue",123,104,238},
    {"mediumspringgreen",0,250,154},{"mediumturquoise",72,209,204},
    {"mediumvioletred",199,21,133},{"midnightblue",25,25,112},
    {"mintcream",245,255,250},{"mistyrose",255,228,225},{"moccasin",255,228,181},
    {"navajowhite",255,222,173},{"navy",0,0,128},{"oldlace",253,245,230},
    {"olive",128,128,0},{"olivedrab",107,142,35},{"orange",255,165,0},
    {"orangered",255,69,0},{"orchid",218,112,214},{"palegoldenrod",238,232,170},
    {"palegreen",152,251,152},{"paleturquoise",175,238,238},{"palevioletred",219,112,147},
    {"papayawhip",255,239,213},{"peachpuff",255,218,185},{"peru",205,133,63},
    {"pink",255,192,203},{"plum",221,160,221},{"powderblue",176,224,230},
    {"purple",128,0,128},{"red",255,0,0},{"rosybrown",188,143,143},
    {"royalblue",65,105,225},{"saddlebrown",139,69,19},{"salmon",250,128,114},
    {"sandybrown",244,164,96},{"seagreen",46,139,87},{"seashell",255,245,238},
    {"sienna",160,82,45},{"silver",192,192,192},{"skyblue",135,206,235},
    {"slateblue",106,90,205},{"slategray",112,128,144},{"slategrey",112,128,144},
    {"snow",255,250,250},{"springgreen",0,255,127},{"steelblue",70,130,180},
    {"tan",210,180,140},{"teal",0,128,128},{"thistle",216,191,216},
    {"tomato",255,99,71},{"turquoise",64,224,208},{"violet",238,130,238},
    {"wheat",245,222,179},{"white",255,255,255},{"whitesmoke",245,245,245},
    {"yellow",255,255,0},{"yellowgreen",154,205,50},
};
constexpr int32 kNamedColorsCount =
    (int32)(sizeof(kNamedColors) / sizeof(kNamedColors[0]));

}  // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 3 — NkSVGColor / NkSVGTransform (API publique du header)
// ─────────────────────────────────────────────────────────────────────────────

NkSVGColor NkSVGColor::Parse(const char* str) noexcept
{
    if (!str) return Black();
    while (*str && IsSpace(*str)) ++str;
    if (!*str) return Black();
    if (StrCaseCmp(str, "none") == 0)        return None();
    if (StrCaseCmp(str, "transparent") == 0) return Transparent();
    if (StrCaseCmp(str, "currentColor") == 0) return Black();  // pas de cascade currentColor

    // #RGB / #RRGGBB / #RRGGBBAA
    if (*str == '#') {
        ++str;
        int32 len = 0;
        while (str[len] && HexDigit(str[len]) >= 0) ++len;
        NkSVGColor c; c.a = 255;
        if (len == 3) {
            c.r = (uint8)(HexDigit(str[0]) * 17);
            c.g = (uint8)(HexDigit(str[1]) * 17);
            c.b = (uint8)(HexDigit(str[2]) * 17);
        } else if (len == 6 || len == 8) {
            c.r = (uint8)(HexDigit(str[0]) * 16 + HexDigit(str[1]));
            c.g = (uint8)(HexDigit(str[2]) * 16 + HexDigit(str[3]));
            c.b = (uint8)(HexDigit(str[4]) * 16 + HexDigit(str[5]));
            if (len == 8) {
                c.a = (uint8)(HexDigit(str[6]) * 16 + HexDigit(str[7]));
            }
        }
        return c;
    }

    // rgb(r,g,b) / rgba(r,g,b,a)
    if (str[0] == 'r' && str[1] == 'g' && str[2] == 'b') {
        const char* p = str + 3;
        if (*p == 'a') ++p;
        while (*p && *p != '(') ++p;
        if (*p == '(') ++p;
        const float32 r = ParseFloat(p, &p); p = SkipWSComma(p);
        const float32 g = ParseFloat(p, &p); p = SkipWSComma(p);
        const float32 b = ParseFloat(p, &p); p = SkipWSComma(p);
        float32 a = 1.f;
        if (*p && *p != ')') a = ParseFloat(p, &p);
        const auto clamp255 = [](float32 v) -> uint8 {
            if (v < 0.f)   return 0;
            if (v > 255.f) return 255;
            return (uint8)v;
        };
        NkSVGColor c;
        c.r = clamp255(r);
        c.g = clamp255(g);
        c.b = clamp255(b);
        c.a = (a <= 1.f) ? clamp255(a * 255.f) : clamp255(a);
        return c;
    }

    // Nom CSS (recherche lineaire, ~150 entrees).
    for (int32 i = 0; i < kNamedColorsCount; ++i) {
        if (StrCaseCmp(str, kNamedColors[i].name) == 0) {
            return { kNamedColors[i].r, kNamedColors[i].g, kNamedColors[i].b, 255, false };
        }
    }
    return Black();
}

NkSVGTransform NkSVGTransform::Rotate(float32 deg) noexcept
{
    const float32 r = deg * 0.017453292519943295f;
    const float32 c = std::cos(r), s = std::sin(r);
    return { c, s, -s, c, 0, 0 };
}

NkSVGTransform NkSVGTransform::operator*(const NkSVGTransform& o) const noexcept
{
    // (*this) * o : applique o d'abord, puis (*this).
    // En forme matricielle :
    //   [a c e]   [oa oc oe]
    //   [b d f] x [ob od of]
    //   [0 0 1]   [0  0  1 ]
    NkSVGTransform r;
    r.a = a * o.a + c * o.b;
    r.b = b * o.a + d * o.b;
    r.c = a * o.c + c * o.d;
    r.d = b * o.c + d * o.d;
    r.e = a * o.e + c * o.f + e;
    r.f = b * o.e + d * o.f + f;
    return r;
}

NkSVGTransform NkSVGTransform::Parse(const char* str) noexcept
{
    NkSVGTransform result = Identity();
    if (!str) return result;
    const char* p = str;
    while (*p) {
        p = SkipWS(p);
        if (!*p) break;
        // Lit le nom de l'op (matrix/translate/scale/rotate/skewX/skewY).
        char op[16] = {0};
        int32 oi = 0;
        while (*p && *p != '(' && !IsSpace(*p) && oi < 15) op[oi++] = *p++;
        op[oi] = 0;
        p = SkipWS(p);
        if (*p != '(') { if (*p) ++p; continue; }
        ++p;
        // Lit jusqu'a 6 floats.
        float32 args[6] = {0};
        int32 nargs = 0;
        while (*p && *p != ')' && nargs < 6) {
            p = SkipWSComma(p);
            if (*p == ')') break;
            args[nargs++] = ParseFloat(p, &p);
        }
        if (*p == ')') ++p;

        NkSVGTransform t = Identity();
        if (std::strcmp(op, "matrix") == 0 && nargs >= 6) {
            t.a = args[0]; t.b = args[1]; t.c = args[2];
            t.d = args[3]; t.e = args[4]; t.f = args[5];
        } else if (std::strcmp(op, "translate") == 0) {
            t = Translate(args[0], nargs > 1 ? args[1] : 0.f);
        } else if (std::strcmp(op, "scale") == 0) {
            t = Scale(args[0], nargs > 1 ? args[1] : args[0]);
        } else if (std::strcmp(op, "rotate") == 0) {
            if (nargs >= 3) {
                // rotate(angle cx cy) = T(cx,cy) * R(angle) * T(-cx,-cy)
                t = Translate(args[1], args[2])
                  * Rotate(args[0])
                  * Translate(-args[1], -args[2]);
            } else {
                t = Rotate(args[0]);
            }
        } else if (std::strcmp(op, "skewX") == 0) {
            const float32 rd = args[0] * 0.017453292519943295f;
            t.a = 1.f; t.b = 0.f; t.c = std::tan(rd); t.d = 1.f;
        } else if (std::strcmp(op, "skewY") == 0) {
            const float32 rd = args[0] * 0.017453292519943295f;
            t.a = 1.f; t.b = std::tan(rd); t.c = 0.f; t.d = 1.f;
        }
        // SVG : "transform=A B C" applique A puis B puis C, lecture
        // gauche-droite. Donc result = result * t.
        result = result * t;
    }
    return result;
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 4 — Structures internes : Shape + PathBuilder + ParseState
// ═════════════════════════════════════════════════════════════════════════════

namespace {

/// Shape interne = un path flatten en polyligne(s), + style cumule + CTM applique.
/// Les xs/ys sont en COORDONNEES DESTINATION (apres ctm + view->out scaling).
struct Shape {
    NkSVGStyle        style;
    NkVector<float32> xs;
    NkVector<float32> ys;
    NkVector<int32>   contourStart;  // index dans xs/ys
    NkVector<int32>   contourLen;

    Shape() = default;
    // Move uniquement (les NkVector peuvent etre couteux a copier).
    Shape(Shape&&) noexcept = default;
    Shape& operator=(Shape&&) noexcept = default;
    Shape(const Shape&) = delete;
    Shape& operator=(const Shape&) = delete;
};

/// State courant pendant le parsing (push/pop sur <g>).
struct ParseState {
    NkSVGStyle     style = {};
    NkSVGTransform xform = NkSVGTransform::Identity();
};

/// Paire (nom, valeur) d'attribut XML. value pointe dans le pool partage.
struct AttrPair { const char* name; const char* value; };

/// Trouve l'attribut par nom dans la liste (case sensitive).
const char* FindAttr(const AttrPair* attrs, int32 n, const char* name) noexcept
{
    for (int32 i = 0; i < n; ++i) {
        if (std::strcmp(attrs[i].name, name) == 0) return attrs[i].value;
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 5 — Construction des paths : PathBuilder + Bezier flatten + Arc
// ─────────────────────────────────────────────────────────────────────────────

/// Helper qui pousse des segments dans une Shape. Gere les contours (M ouvre,
/// Z ferme) + curseur courant + reflexion ctrl points (S/T).
struct PathBuilder {
    Shape& sh;
    float32 curX = 0, curY = 0;     // position du curseur
    float32 startX = 0, startY = 0; // debut du contour courant
    bool    inContour = false;

    explicit PathBuilder(Shape& s) noexcept : sh(s) {}

    void StartContour(float32 x, float32 y) noexcept
    {
        if (inContour) EndContour();
        sh.contourStart.PushBack((int32)sh.xs.Size());
        sh.contourLen.PushBack(1);
        sh.xs.PushBack(x);
        sh.ys.PushBack(y);
        startX = curX = x;
        startY = curY = y;
        inContour = true;
    }
    void LineTo(float32 x, float32 y) noexcept
    {
        if (!inContour) { StartContour(x, y); return; }
        sh.xs.PushBack(x);
        sh.ys.PushBack(y);
        sh.contourLen[sh.contourLen.Size() - 1]++;
        curX = x; curY = y;
    }
    void EndContour() noexcept { inContour = false; }

    void Close() noexcept
    {
        if (!inContour) return;
        if (std::fabs(curX - startX) > 1e-5f || std::fabs(curY - startY) > 1e-5f) {
            sh.xs.PushBack(startX);
            sh.ys.PushBack(startY);
            sh.contourLen[sh.contourLen.Size() - 1]++;
        }
        curX = startX; curY = startY;
        inContour = false;
    }
};

/// Flatten cubique Bezier par subdivision adaptative (De Casteljau).
/// Tolerance flatness = 0.5 px max d'erreur.
void FlattenCubic(PathBuilder& pb,
                  float32 x0, float32 y0,
                  float32 x1, float32 y1,
                  float32 x2, float32 y2,
                  float32 x3, float32 y3,
                  int32 depth = 0) noexcept
{
    constexpr float32 kTol2 = 0.25f;  // (0.5 px)^2
    constexpr int32   kMaxDepth = 12;
    if (depth >= kMaxDepth) {
        pb.LineTo(x3, y3);
        return;
    }
    // Distance des points de controle a la ligne (x0,y0)-(x3,y3).
    const float32 dx = x3 - x0, dy = y3 - y0;
    const float32 d1 = std::fabs((x1 - x3) * dy - (y1 - y3) * dx);
    const float32 d2 = std::fabs((x2 - x3) * dy - (y2 - y3) * dx);
    const float32 len2 = dx * dx + dy * dy;
    if ((d1 + d2) * (d1 + d2) <= kTol2 * len2) {
        pb.LineTo(x3, y3);
        return;
    }
    // Subdivise en 2 par De Casteljau.
    const float32 x01  = (x0 + x1) * 0.5f, y01  = (y0 + y1) * 0.5f;
    const float32 x12  = (x1 + x2) * 0.5f, y12  = (y1 + y2) * 0.5f;
    const float32 x23  = (x2 + x3) * 0.5f, y23  = (y2 + y3) * 0.5f;
    const float32 x012 = (x01 + x12) * 0.5f, y012 = (y01 + y12) * 0.5f;
    const float32 x123 = (x12 + x23) * 0.5f, y123 = (y12 + y23) * 0.5f;
    const float32 xC   = (x012 + x123) * 0.5f, yC = (y012 + y123) * 0.5f;
    FlattenCubic(pb, x0, y0, x01, y01, x012, y012, xC, yC, depth + 1);
    FlattenCubic(pb, xC, yC, x123, y123, x23, y23, x3, y3, depth + 1);
}

/// Flatten quadratique Bezier (eleve en cubique : c1 = p0+2/3(p1-p0), c2 = p2+2/3(p1-p2)).
void FlattenQuad(PathBuilder& pb,
                 float32 x0, float32 y0,
                 float32 x1, float32 y1,
                 float32 x2, float32 y2) noexcept
{
    const float32 cx1 = x0 + (2.f / 3.f) * (x1 - x0);
    const float32 cy1 = y0 + (2.f / 3.f) * (y1 - y0);
    const float32 cx2 = x2 + (2.f / 3.f) * (x1 - x2);
    const float32 cy2 = y2 + (2.f / 3.f) * (y1 - y2);
    FlattenCubic(pb, x0, y0, cx1, cy1, cx2, cy2, x2, y2);
}

/// Convertit un arc elliptique en sequence de Beziers cubiques (W3C SVG 1.1).
/// @p x1,y1 = position depart, @p x2,y2 = position arrivee.
void FlattenArc(PathBuilder& pb,
                float32 x1, float32 y1,
                float32 rx, float32 ry, float32 angleDeg,
                bool largeArc, bool sweep,
                float32 x2, float32 y2) noexcept
{
    if (rx == 0.f || ry == 0.f) { pb.LineTo(x2, y2); return; }
    rx = std::fabs(rx); ry = std::fabs(ry);
    const float32 rad  = angleDeg * 0.017453292519943295f;
    const float32 cosA = std::cos(rad), sinA = std::sin(rad);
    // Step 1 : (x1', y1')
    const float32 dx2 = (x1 - x2) * 0.5f, dy2 = (y1 - y2) * 0.5f;
    const float32 x1p =  cosA * dx2 + sinA * dy2;
    const float32 y1p = -sinA * dx2 + cosA * dy2;
    // Step 2 : (cx', cy')
    float32 rxs = rx * rx, rys = ry * ry;
    const float32 x1ps = x1p * x1p, y1ps = y1p * y1p;
    const float32 cr = x1ps / rxs + y1ps / rys;
    if (cr > 1.f) {
        const float32 s = std::sqrt(cr);
        rx *= s; ry *= s;
        rxs = rx * rx; rys = ry * ry;
    }
    float32 sq = (rxs * rys - rxs * y1ps - rys * x1ps) / (rxs * y1ps + rys * x1ps);
    sq = (sq < 0.f) ? 0.f : sq;
    const float32 coef = (largeArc == sweep ? -1.f : 1.f) * std::sqrt(sq);
    const float32 cxp =  coef * (rx * y1p) / ry;
    const float32 cyp = -coef * (ry * x1p) / rx;
    // Step 3 : (cx, cy)
    const float32 cx = cosA * cxp - sinA * cyp + (x1 + x2) * 0.5f;
    const float32 cy = sinA * cxp + cosA * cyp + (y1 + y2) * 0.5f;
    // Step 4 : theta1 + dtheta
    auto angleFn = [](float32 ux, float32 uy, float32 vx, float32 vy) {
        const float32 dot = ux * vx + uy * vy;
        const float32 lenSq = (ux*ux + uy*uy) * (vx*vx + vy*vy);
        if (lenSq <= 0.f) return 0.f;
        float32 a = std::acos(std::fmax(-1.f, std::fmin(1.f, dot / std::sqrt(lenSq))));
        if (ux * vy - uy * vx < 0.f) a = -a;
        return a;
    };
    const float32 ux1 = (x1p - cxp) / rx, uy1 = (y1p - cyp) / ry;
    const float32 ux2 = (-x1p - cxp) / rx, uy2 = (-y1p - cyp) / ry;
    const float32 theta1 = angleFn(1, 0, ux1, uy1);
    float32 dtheta = angleFn(ux1, uy1, ux2, uy2);
    if (!sweep && dtheta > 0)      dtheta -= 6.283185307179586f;
    else if (sweep && dtheta < 0)  dtheta += 6.283185307179586f;
    // Step 5 : approxime par 1 Bezier par quart de tour max.
    const int32 nSegs = (int32)std::ceil(std::fabs(dtheta) / 1.5707963267948966f);
    if (nSegs <= 0) return;
    const float32 dt = dtheta / (float32)nSegs;
    const float32 t  = (4.f / 3.f) * std::tan(dt * 0.25f);
    float32 cxLast = x1, cyLast = y1;
    for (int32 i = 0; i < nSegs; ++i) {
        const float32 a0 = theta1 + dt * (float32)i;
        const float32 a1 = theta1 + dt * (float32)(i + 1);
        const float32 ca0 = std::cos(a0), sa0 = std::sin(a0);
        const float32 ca1 = std::cos(a1), sa1 = std::sin(a1);
        const float32 ex = cosA * (rx * ca1) - sinA * (ry * sa1) + cx;
        const float32 ey = sinA * (rx * ca1) + cosA * (ry * sa1) + cy;
        const float32 c1x = cosA * (rx * (ca0 - t * sa0))
                          - sinA * (ry * (sa0 + t * ca0)) + cx;
        const float32 c1y = sinA * (rx * (ca0 - t * sa0))
                          + cosA * (ry * (sa0 + t * ca0)) + cy;
        const float32 c2x = cosA * (rx * (ca1 + t * sa1))
                          - sinA * (ry * (sa1 - t * ca1)) + cx;
        const float32 c2y = sinA * (rx * (ca1 + t * sa1))
                          + cosA * (ry * (sa1 - t * ca1)) + cy;
        FlattenCubic(pb, cxLast, cyLast, c1x, c1y, c2x, c2y, ex, ey);
        cxLast = ex; cyLast = ey;
    }
}

/// Parser de l'attribut d="..." pour <path>.
/// Decode toutes les commandes SVG, applique relative/absolue, gere les
/// repetitions implicites (M suivi de coords = ML, M etc.).
void ParsePathD(const char* d, PathBuilder& pb) noexcept
{
    if (!d) return;
    const char* p = d;
    float32 prevCtrlX = 0, prevCtrlY = 0;
    char lastCmd = 0;
    while (*p) {
        p = SkipWSComma(p);
        if (!*p) break;
        char cmd = *p;
        const bool isCmd = (cmd >= 'A' && cmd <= 'Z') || (cmd >= 'a' && cmd <= 'z');
        if (isCmd) {
            ++p;
        } else {
            // Repetition implicite : M -> L (M devient L pour les sets suivants),
            // m -> l. Les autres conservent leur commande.
            cmd = lastCmd;
            if (cmd == 'M') cmd = 'L';
            else if (cmd == 'm') cmd = 'l';
        }
        p = SkipWSComma(p);
        const bool rel = (cmd >= 'a' && cmd <= 'z');
        auto X = [&](float32 x) { return rel ? pb.curX + x : x; };
        auto Y = [&](float32 y) { return rel ? pb.curY + y : y; };
        switch (cmd) {
        case 'M': case 'm': {
            const float32 x = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y = ParseFloat(p, &p);
            pb.StartContour(X(x), Y(y));
            prevCtrlX = pb.curX; prevCtrlY = pb.curY;
            break;
        }
        case 'L': case 'l': {
            const float32 x = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y = ParseFloat(p, &p);
            pb.LineTo(X(x), Y(y));
            prevCtrlX = pb.curX; prevCtrlY = pb.curY;
            break;
        }
        case 'H': case 'h': {
            const float32 x = ParseFloat(p, &p);
            pb.LineTo(X(x), pb.curY);
            prevCtrlX = pb.curX; prevCtrlY = pb.curY;
            break;
        }
        case 'V': case 'v': {
            const float32 y = ParseFloat(p, &p);
            pb.LineTo(pb.curX, Y(y));
            prevCtrlX = pb.curX; prevCtrlY = pb.curY;
            break;
        }
        case 'C': case 'c': {
            const float32 x1 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y1 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 x2 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y2 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 x3 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y3 = ParseFloat(p, &p);
            const float32 sx = pb.curX, sy = pb.curY;
            FlattenCubic(pb, sx, sy,
                         X(x1), Y(y1), X(x2), Y(y2), X(x3), Y(y3));
            prevCtrlX = X(x2); prevCtrlY = Y(y2);
            break;
        }
        case 'S': case 's': {
            const float32 x2 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y2 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 x3 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y3 = ParseFloat(p, &p);
            const float32 sx = pb.curX, sy = pb.curY;
            const bool reflect = (lastCmd == 'C' || lastCmd == 'c'
                               || lastCmd == 'S' || lastCmd == 's');
            const float32 c1x = reflect ? (2.f * sx - prevCtrlX) : sx;
            const float32 c1y = reflect ? (2.f * sy - prevCtrlY) : sy;
            FlattenCubic(pb, sx, sy, c1x, c1y, X(x2), Y(y2), X(x3), Y(y3));
            prevCtrlX = X(x2); prevCtrlY = Y(y2);
            break;
        }
        case 'Q': case 'q': {
            const float32 x1 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y1 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 x2 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y2 = ParseFloat(p, &p);
            const float32 sx = pb.curX, sy = pb.curY;
            FlattenQuad(pb, sx, sy, X(x1), Y(y1), X(x2), Y(y2));
            prevCtrlX = X(x1); prevCtrlY = Y(y1);
            break;
        }
        case 'T': case 't': {
            const float32 x2 = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y2 = ParseFloat(p, &p);
            const float32 sx = pb.curX, sy = pb.curY;
            const bool reflect = (lastCmd == 'Q' || lastCmd == 'q'
                               || lastCmd == 'T' || lastCmd == 't');
            const float32 c1x = reflect ? (2.f * sx - prevCtrlX) : sx;
            const float32 c1y = reflect ? (2.f * sy - prevCtrlY) : sy;
            FlattenQuad(pb, sx, sy, c1x, c1y, X(x2), Y(y2));
            prevCtrlX = c1x; prevCtrlY = c1y;
            break;
        }
        case 'A': case 'a': {
            const float32 rx  = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 ry  = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 ang = ParseFloat(p, &p); p = SkipWSComma(p);
            const int32 large = (int32)ParseFloat(p, &p); p = SkipWSComma(p);
            const int32 sweep = (int32)ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 x = ParseFloat(p, &p); p = SkipWSComma(p);
            const float32 y = ParseFloat(p, &p);
            const float32 sx = pb.curX, sy = pb.curY;
            const float32 ex = X(x), ey = Y(y);
            FlattenArc(pb, sx, sy, rx, ry, ang, large != 0, sweep != 0, ex, ey);
            pb.curX = ex; pb.curY = ey;
            if (pb.sh.contourLen.Size() > 0) {
                // Update du contourLen pour le dernier point d'arc.
                // FlattenArc -> FlattenCubic -> LineTo qui incremente contourLen,
                // donc rien a faire ici. Mais on met a jour curX/curY au-dessus
                // pour les commandes relatives suivantes.
            }
            prevCtrlX = pb.curX; prevCtrlY = pb.curY;
            break;
        }
        case 'Z': case 'z': {
            pb.Close();
            break;
        }
        default:
            // commande inconnue : on saute un caractere et continue
            ++p;
            break;
        }
        lastCmd = cmd;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 6 — Style : application des attributs CSS / individuels
// ─────────────────────────────────────────────────────────────────────────────

void ApplyAttrToStyle(NkSVGStyle& s, const char* name, const char* value) noexcept
{
    if (!name || !value) return;
    if      (std::strcmp(name, "fill") == 0)           s.fill = NkSVGColor::Parse(value);
    else if (std::strcmp(name, "stroke") == 0)         s.stroke = NkSVGColor::Parse(value);
    else if (std::strcmp(name, "stroke-width") == 0)   s.strokeWidth = ParseFloat(value);
    else if (std::strcmp(name, "opacity") == 0)        s.opacity = ParseFloat(value);
    else if (std::strcmp(name, "fill-opacity") == 0)   s.fillOpacity = ParseFloat(value);
    else if (std::strcmp(name, "stroke-opacity") == 0) s.strokeOpacity = ParseFloat(value);
    else if (std::strcmp(name, "fill-rule") == 0)      s.fillEvenOdd = (std::strcmp(value, "evenodd") == 0);
    else if (std::strcmp(name, "display") == 0)        s.visible = std::strcmp(value, "none") != 0;
    else if (std::strcmp(name, "visibility") == 0)     s.visible = std::strcmp(value, "hidden") != 0;
}

/// Parse l'attribut "style" CSS inline (key:value;key:value).
void ApplyCSSStyle(NkSVGStyle& s, const char* css) noexcept
{
    if (!css) return;
    const char* p = css;
    while (*p) {
        // Lit le nom de la propriete.
        char name[64];
        int32 ni = 0;
        while (*p && *p != ':' && *p != ';' && ni + 1 < (int32)sizeof(name)) {
            if (!IsSpace(*p)) name[ni++] = *p;
            ++p;
        }
        name[ni] = 0;
        if (*p != ':') {
            while (*p && *p != ';') ++p;
            if (*p) ++p;
            continue;
        }
        ++p;
        while (IsSpace(*p)) ++p;
        // Lit la valeur (peut etre longue : rgb(...) etc., on prend 255 chars).
        char val[256];
        int32 vi = 0;
        while (*p && *p != ';' && vi + 1 < (int32)sizeof(val)) val[vi++] = *p++;
        val[vi] = 0;
        // Trim trailing whitespace.
        while (vi > 0 && IsSpace(val[vi - 1])) val[--vi] = 0;
        ApplyAttrToStyle(s, name, val);
        if (*p == ';') ++p;
    }
}

/// Fusionne les attributs d'un element sur un style herite.
/// Ordre : (1) parent, (2) attribut "style" inline, (3) attributs individuels.
NkSVGStyle MergeStyle(const NkSVGStyle& parent,
                      const AttrPair* attrs, int32 n) noexcept
{
    NkSVGStyle s = parent;
    const char* css = FindAttr(attrs, n, "style");
    if (css) ApplyCSSStyle(s, css);
    for (int32 i = 0; i < n; ++i) {
        if (std::strcmp(attrs[i].name, "style") == 0) continue;
        ApplyAttrToStyle(s, attrs[i].name, attrs[i].value);
    }
    return s;
}

/// Applique une matrice a tous les points d'une Shape (in-place).
void ApplyTransform(Shape& sh, const NkSVGTransform& m) noexcept
{
    for (uint32 i = 0; i < sh.xs.Size(); ++i) {
        float32 x = sh.xs[i], y = sh.ys[i];
        m.Apply(x, y);
        sh.xs[i] = x;
        sh.ys[i] = y;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 7 — Constructeurs de Shape par tag SVG
// ─────────────────────────────────────────────────────────────────────────────

void ShapeFromRect(NkVector<Shape>& shapes,
                   const AttrPair* a, int32 n, const ParseState& st) noexcept
{
    const char* sw = FindAttr(a, n, "width");
    const char* sh = FindAttr(a, n, "height");
    if (!sw || !sh) return;
    const float32 x = ParseFloat(FindAttr(a, n, "x"));
    const float32 y = ParseFloat(FindAttr(a, n, "y"));
    const float32 w = ParseFloat(sw);
    const float32 h = ParseFloat(sh);
    if (w <= 0 || h <= 0) return;
    Shape s2;
    s2.style = st.style;
    PathBuilder pb(s2);
    pb.StartContour(x,       y);
    pb.LineTo     (x + w,    y);
    pb.LineTo     (x + w, y + h);
    pb.LineTo     (x,     y + h);
    pb.Close();
    ApplyTransform(s2, st.xform);
    shapes.PushBack(std::move(s2));
}

/// Cercle : approxime par 4 Beziers cubiques (k = (4/3)*tan(pi/8) ≈ 0.5523).
void ShapeFromCircle(NkVector<Shape>& shapes,
                     const AttrPair* a, int32 n, const ParseState& st) noexcept
{
    const char* sr = FindAttr(a, n, "r");
    if (!sr) return;
    const float32 cx = ParseFloat(FindAttr(a, n, "cx"));
    const float32 cy = ParseFloat(FindAttr(a, n, "cy"));
    const float32 r  = ParseFloat(sr);
    if (r <= 0) return;
    Shape s2;
    s2.style = st.style;
    PathBuilder pb(s2);
    const float32 k = 0.5522847498307933f;
    pb.StartContour(cx - r, cy);
    FlattenCubic(pb, cx - r, cy,         cx - r,     cy - r * k, cx - r * k, cy - r,     cx,         cy - r);
    FlattenCubic(pb, cx,     cy - r,     cx + r * k, cy - r,     cx + r,     cy - r * k, cx + r,     cy);
    FlattenCubic(pb, cx + r, cy,         cx + r,     cy + r * k, cx + r * k, cy + r,     cx,         cy + r);
    FlattenCubic(pb, cx,     cy + r,     cx - r * k, cy + r,     cx - r,     cy + r * k, cx - r,     cy);
    pb.Close();
    ApplyTransform(s2, st.xform);
    shapes.PushBack(std::move(s2));
}

void ShapeFromEllipse(NkVector<Shape>& shapes,
                      const AttrPair* a, int32 n, const ParseState& st) noexcept
{
    const char* srx = FindAttr(a, n, "rx");
    const char* sry = FindAttr(a, n, "ry");
    if (!srx || !sry) return;
    const float32 cx = ParseFloat(FindAttr(a, n, "cx"));
    const float32 cy = ParseFloat(FindAttr(a, n, "cy"));
    const float32 rx = ParseFloat(srx), ry = ParseFloat(sry);
    if (rx <= 0 || ry <= 0) return;
    Shape s2;
    s2.style = st.style;
    PathBuilder pb(s2);
    const float32 k = 0.5522847498307933f;
    pb.StartContour(cx - rx, cy);
    FlattenCubic(pb, cx - rx, cy,         cx - rx,      cy - ry * k, cx - rx * k, cy - ry,      cx,          cy - ry);
    FlattenCubic(pb, cx,      cy - ry,    cx + rx * k,  cy - ry,     cx + rx,     cy - ry * k,  cx + rx,     cy);
    FlattenCubic(pb, cx + rx, cy,         cx + rx,      cy + ry * k, cx + rx * k, cy + ry,      cx,          cy + ry);
    FlattenCubic(pb, cx,      cy + ry,    cx - rx * k,  cy + ry,     cx - rx,     cy + ry * k,  cx - rx,     cy);
    pb.Close();
    ApplyTransform(s2, st.xform);
    shapes.PushBack(std::move(s2));
}

void ShapeFromLine(NkVector<Shape>& shapes,
                   const AttrPair* a, int32 n, const ParseState& st) noexcept
{
    Shape s2;
    s2.style = st.style;
    PathBuilder pb(s2);
    pb.StartContour(ParseFloat(FindAttr(a, n, "x1")),
                    ParseFloat(FindAttr(a, n, "y1")));
    pb.LineTo      (ParseFloat(FindAttr(a, n, "x2")),
                    ParseFloat(FindAttr(a, n, "y2")));
    ApplyTransform(s2, st.xform);
    shapes.PushBack(std::move(s2));
}

void ShapeFromPolygon(NkVector<Shape>& shapes,
                      const AttrPair* a, int32 n, const ParseState& st,
                      bool closed) noexcept
{
    const char* pts = FindAttr(a, n, "points");
    if (!pts) return;
    Shape s2;
    s2.style = st.style;
    PathBuilder pb(s2);
    const char* p = pts;
    bool first = true;
    while (*p) {
        p = SkipWSComma(p);
        if (!*p) break;
        const float32 x = ParseFloat(p, &p); p = SkipWSComma(p);
        if (!*p && first) break;
        const float32 y = ParseFloat(p, &p);
        if (first) { pb.StartContour(x, y); first = false; }
        else        pb.LineTo(x, y);
    }
    if (closed) pb.Close();
    ApplyTransform(s2, st.xform);
    shapes.PushBack(std::move(s2));
}

void ShapeFromPath(NkVector<Shape>& shapes,
                   const AttrPair* a, int32 n, const ParseState& st) noexcept
{
    const char* d = FindAttr(a, n, "d");
    if (!d) return;
    Shape s2;
    s2.style = st.style;
    PathBuilder pb(s2);
    ParsePathD(d, pb);
    ApplyTransform(s2, st.xform);
    shapes.PushBack(std::move(s2));
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 8 — Parser XML stream-based avec stack <g>
// ─────────────────────────────────────────────────────────────────────────────

/// Parse les attributs d'une balise jusqu'a '>'. Les noms et valeurs sont
/// stockes dans des buffers fournis par le caller (nameBuf + attrPool partage).
int32 ParseTagAttrs(const char*& p, const char* end,
                    AttrPair* attrs,
                    char* nameBuf, usize& nameOff, usize nameBufSize,
                    char* attrPool, usize& poolOff, usize attrPoolSize) noexcept
{
    int32 num = 0;
    while (p < end && *p != '>' && *p != '/') {
        while (p < end && IsSpace(*p)) ++p;
        if (p >= end || *p == '>' || *p == '/') break;
        // Nom de l'attribut.
        const usize nameStart = nameOff;
        while (p < end && *p != '=' && !IsSpace(*p) && *p != '>' && *p != '/'
            && nameOff + 1 < nameBufSize) {
            nameBuf[nameOff++] = *p++;
        }
        if (nameOff < nameBufSize) nameBuf[nameOff++] = 0;
        while (p < end && IsSpace(*p)) ++p;
        // Valeur.
        const char* valPtr = "";
        if (p < end && *p == '=') {
            ++p;
            while (p < end && IsSpace(*p)) ++p;
            const char q = (*p == '"' || *p == '\'') ? *p++ : ' ';
            valPtr = (const char*)(attrPool + poolOff);
            usize vi = 0;
            while (p < end && *p != q && *p != '>'
                && poolOff + vi + 1 < attrPoolSize) {
                attrPool[poolOff + vi++] = *p++;
            }
            // Si pool sature, skip le reste pour avancer correctement.
            while (p < end && *p != q && *p != '>') ++p;
            attrPool[poolOff + vi] = 0;
            poolOff += vi + 1;
            if (*p == q) ++p;
        }
        if (num < 64) {
            attrs[num].name  = nameBuf + nameStart;
            attrs[num].value = valPtr;
            ++num;
        }
    }
    return num;
}

/// Lit le prochain tag dans le flux XML.
/// @return 0 = fin, 1 = balise ouvrante, 2 = balise self-close, 3 = balise fermante.
int32 ReadNextTag(const char*& p, const char* end,
                  char* tagBuf, usize tagBufSize,
                  AttrPair* attrs, int32& outNumAttrs,
                  char* nameBuf, usize& nameOff, usize nameBufSize,
                  char* attrPool, usize& poolOff, usize attrPoolSize) noexcept
{
    while (p < end) {
        // Skip texte / espace entre tags.
        while (p < end && *p != '<') ++p;
        if (p >= end) return 0;
        ++p;
        // Commentaire <!-- ... -->
        if (p + 2 < end && p[0] == '!' && p[1] == '-' && p[2] == '-') {
            p += 3;
            while (p + 2 < end && !(p[0] == '-' && p[1] == '-' && p[2] == '>')) ++p;
            if (p + 2 < end) p += 3;
            continue;
        }
        // PI <?...?> ou DOCTYPE <!...>
        if (p < end && (*p == '?' || *p == '!')) {
            while (p < end && *p != '>') ++p;
            if (p < end) ++p;
            continue;
        }
        // Closing tag </tag>
        if (p < end && *p == '/') {
            ++p;
            usize ti = 0;
            while (p < end && *p != '>' && !IsSpace(*p) && ti + 1 < tagBufSize) {
                tagBuf[ti++] = *p++;
            }
            tagBuf[ti] = 0;
            while (p < end && *p != '>') ++p;
            if (p < end) ++p;
            outNumAttrs = 0;
            return 3;
        }
        // Opening tag <tag attr=...>
        usize ti = 0;
        while (p < end && *p != '>' && *p != '/' && !IsSpace(*p)
            && ti + 1 < tagBufSize) {
            tagBuf[ti++] = *p++;
        }
        tagBuf[ti] = 0;
        // On reset les offsets de pool a chaque tag (les valeurs precedentes
        // ne sont plus utilisees apres traitement du tag).
        nameOff = 0; poolOff = 0;
        outNumAttrs = ParseTagAttrs(p, end, attrs,
                                    nameBuf, nameOff, nameBufSize,
                                    attrPool, poolOff, attrPoolSize);
        const bool self = (p < end && *p == '/');
        while (p < end && *p != '>') ++p;
        if (p < end) ++p;
        return self ? 2 : 1;
    }
    return 0;
}

/// Parse l'ensemble du document SVG -> liste de shapes + viewBox + dimensions.
void ParseSVGDocument(const char* xml, usize xmlLen,
                      NkVector<Shape>& shapes,
                      float32& vbX, float32& vbY, float32& vbW, float32& vbH,
                      float32& svgW, float32& svgH) noexcept
{
    vbX = vbY = 0.f; vbW = 0.f; vbH = 0.f; svgW = 0.f; svgH = 0.f;

    constexpr usize kAttrPoolSize = 1024 * 1024;  // 1 MB par tag
    constexpr usize kNameBufSize  = 64 * 1024;    // 64 KB noms d'attrs
    char* nameBuf  = (char*)NkAlloc(kNameBufSize);
    char* attrPool = (char*)NkAlloc(kAttrPoolSize);
    if (!nameBuf || !attrPool) {
        if (nameBuf)  NkFree(nameBuf);
        if (attrPool) NkFree(attrPool);
        return;
    }
    usize nameOff = 0, poolOff = 0;

    char tagBuf[64];
    AttrPair attrs[64];

    // Stack <g> avec depth max raisonnable.
    constexpr int32 kMaxDepth = 64;
    ParseState stack[kMaxDepth];
    int32 depth = 0;
    stack[0] = {};

    int32 numAttrs = 0;
    bool gotSvg = false;
    // BUG FIX : end est calcule UNE seule fois. xml est passe par reference et
    // est avance par le parser ; si on recalcule "xml + xmlLen" a chaque tour,
    // end se decale avec xml et finit par sortir du buffer (SEGV sur memoire
    // non mappee lors du parsing de longs SVG).
    const char* const xmlEnd = xml + xmlLen;
    while (true) {
        const int32 kind = ReadNextTag(reinterpret_cast<const char*&>(xml),
                                       xmlEnd,
                                       tagBuf, sizeof(tagBuf),
                                       attrs, numAttrs,
                                       nameBuf, nameOff, kNameBufSize,
                                       attrPool, poolOff, kAttrPoolSize);
        if (kind == 0) break;

        if (kind == 3) {
            // Closing tag : depile la stack si c'est </g> ou </svg>.
            if (std::strcmp(tagBuf, "g") == 0 && depth > 0) {
                --depth;
            }
            continue;
        }

        ParseState& cur = stack[depth];

        if (std::strcmp(tagBuf, "svg") == 0 && !gotSvg) {
            const char* vb = FindAttr(attrs, numAttrs, "viewBox");
            if (vb) {
                const char* pp = vb;
                vbX = ParseFloat(pp, &pp); pp = SkipWSComma(pp);
                vbY = ParseFloat(pp, &pp); pp = SkipWSComma(pp);
                vbW = ParseFloat(pp, &pp); pp = SkipWSComma(pp);
                vbH = ParseFloat(pp, &pp);
            }
            const char* w = FindAttr(attrs, numAttrs, "width");
            const char* h = FindAttr(attrs, numAttrs, "height");
            if (w) svgW = ParseFloat(w);
            if (h) svgH = ParseFloat(h);
            // <svg> peut aussi porter fill/stroke par defaut + transform.
            cur.style = MergeStyle(cur.style, attrs, numAttrs);
            const char* tr = FindAttr(attrs, numAttrs, "transform");
            if (tr) cur.xform = cur.xform * NkSVGTransform::Parse(tr);
            gotSvg = true;
            continue;
        }

        if (std::strcmp(tagBuf, "g") == 0) {
            // Ouverture <g> : push un nouveau state derive de cur.
            // BUG FIX : on ne pop QU'A la fermeture </g>, pas a la sortie du
            // tag courant. Les <path> enfants heritent ainsi du transform/style.
            if (kind == 1 && depth + 1 < kMaxDepth) {
                ParseState next = cur;
                next.style = MergeStyle(cur.style, attrs, numAttrs);
                const char* tr = FindAttr(attrs, numAttrs, "transform");
                if (tr) next.xform = cur.xform * NkSVGTransform::Parse(tr);
                stack[++depth] = next;
            }
            // <g/> self-closed = groupe vide, on ignore.
            continue;
        }

        // Tags structurels ignores (contenu sans shape direct).
        if (std::strcmp(tagBuf, "defs")     == 0
         || std::strcmp(tagBuf, "title")    == 0
         || std::strcmp(tagBuf, "desc")     == 0
         || std::strcmp(tagBuf, "metadata") == 0
         || std::strcmp(tagBuf, "style")    == 0)
        {
            // Skip jusqu'au closing equivalent.
            if (kind == 1) {
                int32 nested = 1;
                while (nested > 0) {
                    const int32 k2 = ReadNextTag(reinterpret_cast<const char*&>(xml),
                                                 xml + xmlLen,
                                                 tagBuf, sizeof(tagBuf),
                                                 attrs, numAttrs,
                                                 nameBuf, nameOff, kNameBufSize,
                                                 attrPool, poolOff, kAttrPoolSize);
                    if (k2 == 0) break;
                    if (k2 == 1) ++nested;
                    else if (k2 == 3) --nested;
                }
            }
            continue;
        }

        // Element shape : compose le state local = cur + attrs propres.
        ParseState local = cur;
        local.style = MergeStyle(cur.style, attrs, numAttrs);
        const char* tr = FindAttr(attrs, numAttrs, "transform");
        if (tr) local.xform = cur.xform * NkSVGTransform::Parse(tr);

        if      (std::strcmp(tagBuf, "rect") == 0)     ShapeFromRect    (shapes, attrs, numAttrs, local);
        else if (std::strcmp(tagBuf, "circle") == 0)   ShapeFromCircle  (shapes, attrs, numAttrs, local);
        else if (std::strcmp(tagBuf, "ellipse") == 0)  ShapeFromEllipse (shapes, attrs, numAttrs, local);
        else if (std::strcmp(tagBuf, "line") == 0)     ShapeFromLine    (shapes, attrs, numAttrs, local);
        else if (std::strcmp(tagBuf, "polyline") == 0) ShapeFromPolygon (shapes, attrs, numAttrs, local, false);
        else if (std::strcmp(tagBuf, "polygon")  == 0) ShapeFromPolygon (shapes, attrs, numAttrs, local, true);
        else if (std::strcmp(tagBuf, "path") == 0)     ShapeFromPath    (shapes, attrs, numAttrs, local);
        // Autres tags ignores.
    }

    NkFree(nameBuf);
    NkFree(attrPool);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 9 — Rasterizer scanline AA (supersample 2x)
// ─────────────────────────────────────────────────────────────────────────────

struct ScanEdge { float32 x; int32 dir; };

void RasterizeShape(NkImage& img, const Shape& sh) noexcept
{
    if (sh.contourStart.IsEmpty()) return;
    const NkSVGColor& fill = sh.style.fill;
    if (fill.none) return;
    if (!sh.style.visible) return;

    // Alpha effectif = fill.a * opacity * fill-opacity (range 0..1).
    const float32 alphaF = (float32)fill.a / 255.f
                         * sh.style.opacity * sh.style.fillOpacity;
    if (alphaF <= 0.f) return;
    const uint8 fillAlpha = (uint8)(alphaF * 255.f);

    const int32 W = img.Width();
    const int32 H = img.Height();
    if (W <= 0 || H <= 0) return;

    // BBox du shape pour limiter les scanlines.
    float32 minY =  1e30f, maxY = -1e30f;
    for (uint32 i = 0; i < sh.ys.Size(); ++i) {
        if (sh.ys[i] < minY) minY = sh.ys[i];
        if (sh.ys[i] > maxY) maxY = sh.ys[i];
    }
    int32 y0 = (int32)std::floor(minY);
    int32 y1 = (int32)std::ceil(maxY);
    if (y0 < 0) y0 = 0;
    if (y1 > H) y1 = H;
    if (y0 >= y1) return;

    // Supersample factor vertical pour AA edges.
    constexpr int32 kSS = 2;

    // Buffer de couverture par pixel (0..255) pour la ligne courante.
    uint8* covRow = (uint8*)NkAlloc((usize)W);
    if (!covRow) return;

    uint8* pixels = img.Pixels();
    const int32 stride = img.Stride();
    const bool evenOdd = sh.style.fillEvenOdd;

    constexpr int32 kMaxEdges = 4096;
    ScanEdge edges[kMaxEdges];

    for (int32 py = y0; py < y1; ++py) {
        std::memset(covRow, 0, (usize)W);

        // Pour chaque sub-scanline (2x AA).
        for (int32 ss = 0; ss < kSS; ++ss) {
            const float32 ySub = (float32)py + ((float32)ss + 0.5f) / (float32)kSS;
            int32 nEdges = 0;
            // Collecte les intersections de tous les segments.
            for (uint32 ci = 0; ci < sh.contourStart.Size(); ++ci) {
                const int32 start = sh.contourStart[ci];
                const int32 len   = sh.contourLen[ci];
                if (len < 2) continue;
                for (int32 k = 0; k < len - 1; ++k) {
                    const float32 xA = sh.xs[start + k];
                    const float32 yA = sh.ys[start + k];
                    const float32 xB = sh.xs[start + k + 1];
                    const float32 yB = sh.ys[start + k + 1];
                    // Segment cross la sub-scanline ?
                    if ((yA <= ySub && yB > ySub) || (yB <= ySub && yA > ySub)) {
                        const float32 t = (ySub - yA) / (yB - yA);
                        const float32 x = xA + (xB - xA) * t;
                        if (nEdges < kMaxEdges) {
                            edges[nEdges].x   = x;
                            edges[nEdges].dir = (yA <= ySub) ? +1 : -1;
                            ++nEdges;
                        }
                    }
                }
            }
            if (nEdges < 2) continue;
            // Tri par x croissant (insertion sort, nEdges typiquement petit).
            for (int32 i = 1; i < nEdges; ++i) {
                ScanEdge tmp = edges[i];
                int32 j = i - 1;
                while (j >= 0 && edges[j].x > tmp.x) { edges[j+1] = edges[j]; --j; }
                edges[j+1] = tmp;
            }

            // Remplit selon fill-rule.
            if (evenOdd) {
                for (int32 i = 0; i + 1 < nEdges; i += 2) {
                    float32 xL = edges[i].x;
                    float32 xR = edges[i+1].x;
                    if (xL > xR) std::swap(xL, xR);
                    int32 ixL = (int32)std::floor(xL);
                    int32 ixR = (int32)std::ceil(xR);
                    if (ixL < 0) ixL = 0;
                    if (ixR > W) ixR = W;
                    for (int32 x = ixL; x < ixR; ++x) {
                        const float32 pL = (float32)x;
                        const float32 pR = (float32)(x + 1);
                        float32 cov = std::fmin(pR, xR) - std::fmax(pL, xL);
                        if (cov <= 0.f) continue;
                        if (cov > 1.f) cov = 1.f;
                        const int32 add = (int32)(cov * (255.f / (float32)kSS));
                        const int32 s = (int32)covRow[x] + add;
                        covRow[x] = (uint8)(s > 255 ? 255 : s);
                    }
                }
            } else {
                // Non-zero : winding counter.
                int32 winding = 0;
                float32 spanStart = 0.f;
                bool open = false;
                for (int32 i = 0; i < nEdges; ++i) {
                    const int32 prev = winding;
                    winding += edges[i].dir;
                    if (prev == 0 && winding != 0) {
                        spanStart = edges[i].x;
                        open = true;
                    } else if (prev != 0 && winding == 0 && open) {
                        float32 xL = spanStart;
                        float32 xR = edges[i].x;
                        if (xL > xR) std::swap(xL, xR);
                        int32 ixL = (int32)std::floor(xL);
                        int32 ixR = (int32)std::ceil(xR);
                        if (ixL < 0) ixL = 0;
                        if (ixR > W) ixR = W;
                        for (int32 x = ixL; x < ixR; ++x) {
                            const float32 pL = (float32)x;
                            const float32 pR = (float32)(x + 1);
                            float32 cov = std::fmin(pR, xR) - std::fmax(pL, xL);
                            if (cov <= 0.f) continue;
                            if (cov > 1.f) cov = 1.f;
                            const int32 add = (int32)(cov * (255.f / (float32)kSS));
                            const int32 s = (int32)covRow[x] + add;
                            covRow[x] = (uint8)(s > 255 ? 255 : s);
                        }
                        open = false;
                    }
                }
            }
        }

        // Blend la row dans l'image (premultiplied alpha src-over).
        uint8* dst = pixels + (usize)py * (usize)stride;
        for (int32 x = 0; x < W; ++x) {
            const uint8 c = covRow[x];
            if (c == 0) continue;
            const float32 alpha = (float32)fillAlpha / 255.f
                                * (float32)c / 255.f;
            const int32 sa = (int32)(alpha * 255.f);
            const int32 invA = 255 - sa;
            uint8* px = dst + (usize)x * 4;
            px[0] = (uint8)(((int32)fill.r * sa + (int32)px[0] * invA + 127) / 255);
            px[1] = (uint8)(((int32)fill.g * sa + (int32)px[1] * invA + 127) / 255);
            px[2] = (uint8)(((int32)fill.b * sa + (int32)px[2] * invA + 127) / 255);
            const int32 a2 = (int32)px[3] + sa - ((int32)px[3] * sa + 127) / 255;
            px[3] = (uint8)(a2 > 255 ? 255 : (a2 < 0 ? 0 : a2));
        }
    }

    NkFree(covRow);
}

// ─────────────────────────────────────────────────────────────────────────────
// Construit une "shape ruban" autour du chemin source pour rasteriser le
// stroke. Approche : pour chaque vertex on calcule la bisectrice des
// tangentes adjacentes -> miter joint (limite a 4*hw pour eviter les
// pointes infinies). Cap butt aux extremites des contours ouverts.
//
// Le polygone resultant est rempli par RasterizeShape comme une shape
// ordinaire avec la couleur stroke en "fill". Suffisant pour Phase 2
// (icones, logos, paths simples). Phase 3+ : cap round/square, join round.
// ─────────────────────────────────────────────────────────────────────────────
void BuildStrokeShape(const Shape& src, float32 hwPx, Shape& dst) noexcept
{
    if (hwPx <= 0.f) return;
    constexpr float32 kMiterLimit = 4.f;
    const float32 maxMiter = kMiterLimit * hwPx;

    for (uint32 ci = 0; ci < src.contourStart.Size(); ++ci) {
        const int32 start = src.contourStart[ci];
        const int32 len   = src.contourLen[ci];
        if (len < 2) continue;

        // Contour ferme si premier == dernier point (a 1e-4 pres).
        const bool closed =
            (std::fabs(src.xs[start] - src.xs[start + len - 1]) < 1e-4f &&
             std::fabs(src.ys[start] - src.ys[start + len - 1]) < 1e-4f);
        // Si ferme, on ignore le doublon final pour le calcul des tangentes.
        const int32 nPts = closed ? (len - 1) : len;
        if (nPts < 2) continue;

        NkVector<float32> outerX, outerY;
        NkVector<float32> innerX, innerY;
        outerX.Reserve((uint32)nPts);
        outerY.Reserve((uint32)nPts);
        innerX.Reserve((uint32)nPts);
        innerY.Reserve((uint32)nPts);

        for (int32 i = 0; i < nPts; ++i) {
            const float32 px = src.xs[start + i];
            const float32 py = src.ys[start + i];

            // Tangente entrante (vers vertex precedent).
            float32 tix = 0.f, tiy = 0.f;
            bool hasIn = false;
            if (i > 0 || closed) {
                const int32 prev = (i > 0) ? (i - 1) : (nPts - 1);
                tix = px - src.xs[start + prev];
                tiy = py - src.ys[start + prev];
                const float32 li = std::sqrt(tix*tix + tiy*tiy);
                if (li > 1e-6f) { tix /= li; tiy /= li; hasIn = true; }
            }
            // Tangente sortante (vers vertex suivant).
            float32 tox = 0.f, toy = 0.f;
            bool hasOut = false;
            if (i < nPts - 1 || closed) {
                const int32 next = (i < nPts - 1) ? (i + 1) : 0;
                tox = src.xs[start + next] - px;
                toy = src.ys[start + next] - py;
                const float32 lo = std::sqrt(tox*tox + toy*toy);
                if (lo > 1e-6f) { tox /= lo; toy /= lo; hasOut = true; }
            }

            // Calcul de l'offset normal a ce vertex.
            float32 ox = 0.f, oy = 0.f;
            if (hasIn && hasOut) {
                // Bisectrice des tangentes = direction du joint miter.
                float32 bx = tix + tox, by = tiy + toy;
                const float32 bl = std::sqrt(bx*bx + by*by);
                if (bl < 1e-4f) {
                    // U-turn (180 deg) : tangentes opposees, on prend la perp de tin.
                    ox = -tiy * hwPx;
                    oy =  tix * hwPx;
                } else {
                    bx /= bl; by /= bl;
                    // Miter direction = perp(bisectrice).
                    float32 mx = -by, my = bx;
                    // Longueur miter = hw / cos(angle/2) = hw / dot(miter, perp(tin)).
                    const float32 perpInX = -tiy, perpInY = tix;
                    const float32 d = mx * perpInX + my * perpInY;
                    float32 mlen = hwPx;
                    if (std::fabs(d) > 1e-4f) mlen = hwPx / d;
                    // Clamp miter pour eviter les pointes a l'infini sur angles aigus.
                    if (mlen >  maxMiter) mlen =  maxMiter;
                    if (mlen < -maxMiter) mlen = -maxMiter;
                    ox = mx * mlen;
                    oy = my * mlen;
                }
            } else if (hasIn) {
                // Extremite finale (path ouvert) : cap butt, perp tangente entrante.
                ox = -tiy * hwPx;
                oy =  tix * hwPx;
            } else if (hasOut) {
                // Extremite initiale (path ouvert) : perp tangente sortante.
                ox = -toy * hwPx;
                oy =  tox * hwPx;
            } else {
                continue;
            }

            outerX.PushBack(px + ox);
            outerY.PushBack(py + oy);
            innerX.PushBack(px - ox);
            innerY.PushBack(py - oy);
        }

        if (outerX.Size() < 2) continue;

        // Polygone ferme : outer (sens normal) suivi de inner (sens inverse).
        const int32 cStart = (int32)dst.xs.Size();
        for (uint32 k = 0; k < outerX.Size(); ++k) {
            dst.xs.PushBack(outerX[k]);
            dst.ys.PushBack(outerY[k]);
        }
        for (int32 k = (int32)innerX.Size() - 1; k >= 0; --k) {
            dst.xs.PushBack(innerX[(uint32)k]);
            dst.ys.PushBack(innerY[(uint32)k]);
        }
        // Ferme le polygone (retour au premier vertex outer).
        dst.xs.PushBack(outerX[0]);
        dst.ys.PushBack(outerY[0]);
        const int32 cEnd = (int32)dst.xs.Size();
        dst.contourStart.PushBack(cStart);
        dst.contourLen.PushBack(cEnd - cStart);
    }
}

/// Donnees opaques d'un NkSVGImage : shapes vectorielles + dimensions natives.
/// Pointe par NkSVGImage::mImpl (PIMPL).
struct SVGImageImpl
{
    NkVector<Shape> shapes;
    float32 vbX = 0, vbY = 0, vbW = 0, vbH = 0;
    float32 svgW = 0, svgH = 0;
};

}  // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 9.5 — Triangulation : ear-clipping pour acces mesh 3D / collision
// ═════════════════════════════════════════════════════════════════════════════

namespace {

/// Signed area (algorithm shoelace). Positif = CCW, negatif = CW.
float32 SignedArea(const float32* xs, const float32* ys, int32 n) noexcept
{
    float32 s = 0.f;
    for (int32 i = 0; i < n; ++i) {
        const int32 j = (i + 1) % n;
        s += xs[i] * ys[j] - xs[j] * ys[i];
    }
    return s * 0.5f;
}

/// Test point-in-triangle via signes des aires (barycentric-like).
bool PointInTriangle(float32 px, float32 py,
                     float32 ax, float32 ay,
                     float32 bx, float32 by,
                     float32 cx, float32 cy) noexcept
{
    const float32 d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
    const float32 d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
    const float32 d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
    const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

/// Triangule un contour simple par ear-clipping. Output : indices triangle-list
/// dans le repere original (les indices pointent dans @p xs / @p ys, decales
/// par @p baseIndex). @return nombre de triangles produits.
/// Algo O(N²) -- OK pour contours < ~500 vertices.
int32 EarClipContour(const float32* xs, const float32* ys, int32 n,
                     uint32 baseIndex,
                     NkVector<uint32>& outIndices) noexcept
{
    if (n < 3) return 0;
    // Detection orientation. Si CW, on inverse l'ordre (ear-clipping veut CCW).
    const bool isCCW = (SignedArea(xs, ys, n) > 0.f);

    // Liste de vertices restants (indices dans xs/ys).
    NkVector<int32> verts;
    verts.Reserve((usize)n);
    if (isCCW) {
        for (int32 i = 0; i < n; ++i) verts.PushBack(i);
    } else {
        for (int32 i = n - 1; i >= 0; --i) verts.PushBack(i);
    }

    int32 triCount = 0;
    int32 guard = 2 * n;  // protection anti-boucle infinie (cas degenere)
    while (verts.Size() >= 3 && guard-- > 0) {
        const int32 nv = (int32)verts.Size();
        bool earFound = false;
        for (int32 i = 0; i < nv; ++i) {
            const int32 i0 = verts[(i - 1 + nv) % nv];
            const int32 i1 = verts[i];
            const int32 i2 = verts[(i + 1) % nv];
            const float32 ax = xs[i0], ay = ys[i0];
            const float32 bx = xs[i1], by = ys[i1];
            const float32 cx = xs[i2], cy = ys[i2];
            // Convexite (cross product positif en CCW).
            const float32 cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if (cross <= 0.f) continue;
            // Aucun autre vertex du polygone a l'interieur du triangle ?
            bool inside = false;
            for (int32 k = 0; k < nv && !inside; ++k) {
                const int32 ki = verts[k];
                if (ki == i0 || ki == i1 || ki == i2) continue;
                if (PointInTriangle(xs[ki], ys[ki], ax, ay, bx, by, cx, cy)) {
                    inside = true;
                }
            }
            if (inside) continue;
            // Ear trouve : output triangle + retire i1.
            outIndices.PushBack(baseIndex + (uint32)i0);
            outIndices.PushBack(baseIndex + (uint32)i1);
            outIndices.PushBack(baseIndex + (uint32)i2);
            ++triCount;
            verts.RemoveAt((uint32)i);
            earFound = true;
            break;
        }
        if (!earFound) break;  // Polygone degenere ou non-simple
    }
    return triCount;
}

/// Pack RGBA8 -> uint32 (little-endian : R=byte0, G=byte1, B=byte2, A=byte3).
uint32 PackRGBA(uint8 r, uint8 g, uint8 b, uint8 a) noexcept
{
    return ((uint32)r) | ((uint32)g << 8) | ((uint32)b << 16) | ((uint32)a << 24);
}

}  // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 9.6 — NkSVGShapeView : implementation
// ═════════════════════════════════════════════════════════════════════════════

int32 NkSVGShapeView::ContourCount() const noexcept
{
    const Shape* sh = (const Shape*)mShapePtr;
    return sh ? (int32)sh->contourStart.Size() : 0;
}

int32 NkSVGShapeView::ContourPointCount(int32 ci) const noexcept
{
    const Shape* sh = (const Shape*)mShapePtr;
    if (!sh || ci < 0 || (uint32)ci >= sh->contourLen.Size()) return 0;
    return sh->contourLen[(uint32)ci];
}

const float32* NkSVGShapeView::ContourXs(int32 ci) const noexcept
{
    const Shape* sh = (const Shape*)mShapePtr;
    if (!sh || ci < 0 || (uint32)ci >= sh->contourStart.Size()) return nullptr;
    return &sh->xs[(uint32)sh->contourStart[(uint32)ci]];
}

const float32* NkSVGShapeView::ContourYs(int32 ci) const noexcept
{
    const Shape* sh = (const Shape*)mShapePtr;
    if (!sh || ci < 0 || (uint32)ci >= sh->contourStart.Size()) return nullptr;
    return &sh->ys[(uint32)sh->contourStart[(uint32)ci]];
}

NkSVGColor NkSVGShapeView::FillColor() const noexcept
{
    const Shape* sh = (const Shape*)mShapePtr;
    return sh ? sh->style.fill : NkSVGColor::Black();
}

NkSVGColor NkSVGShapeView::StrokeColor() const noexcept
{
    const Shape* sh = (const Shape*)mShapePtr;
    return sh ? sh->style.stroke : NkSVGColor::None();
}

float32 NkSVGShapeView::StrokeWidth() const noexcept
{
    const Shape* sh = (const Shape*)mShapePtr;
    return sh ? sh->style.strokeWidth : 1.f;
}

float32 NkSVGShapeView::Opacity() const noexcept
{
    const Shape* sh = (const Shape*)mShapePtr;
    return sh ? sh->style.opacity : 1.f;
}

bool NkSVGShapeView::FillEvenOdd() const noexcept
{
    const Shape* sh = (const Shape*)mShapePtr;
    return sh && sh->style.fillEvenOdd;
}

int32 NkSVGShapeView::Triangulate(NkVector<float32>& outXs,
                                  NkVector<float32>& outYs,
                                  NkVector<uint32>&  outIndices,
                                  uint32 baseIndex) const noexcept
{
    const Shape* sh = (const Shape*)mShapePtr;
    if (!sh) return 0;

    // Si baseIndex == 0 et que les buffers de sortie sont vides, on peut
    // commencer a 0. Sinon on utilise baseIndex pour chainer (appel par
    // TriangulateAll en boucle sur les shapes).
    uint32 curBase = baseIndex;
    int32 totalTris = 0;
    for (uint32 ci = 0; ci < sh->contourStart.Size(); ++ci) {
        const int32 start = sh->contourStart[ci];
        const int32 len   = sh->contourLen[ci];
        if (len < 3) continue;
        // Le contour peut etre ferme (dernier point == premier). On detecte
        // et on l'ignore pour l'ear clipping.
        const float32 fx = sh->xs[(uint32)start];
        const float32 fy = sh->ys[(uint32)start];
        const float32 lx = sh->xs[(uint32)(start + len - 1)];
        const float32 ly = sh->ys[(uint32)(start + len - 1)];
        int32 n = len;
        if (std::fabs(fx - lx) < 1e-5f && std::fabs(fy - ly) < 1e-5f) {
            n = len - 1;
        }
        if (n < 3) continue;

        // Copie locale des vertices pour ear-clipping (puis push dans outXs/Ys).
        for (int32 k = 0; k < n; ++k) {
            outXs.PushBack(sh->xs[(uint32)(start + k)]);
            outYs.PushBack(sh->ys[(uint32)(start + k)]);
        }
        const uint32 contourBase = curBase;
        totalTris += EarClipContour(
            &sh->xs[(uint32)start], &sh->ys[(uint32)start], n,
            contourBase, outIndices);
        curBase += (uint32)n;
    }
    return totalTris;
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 10 — NkSVGImage API publique : stockage vectoriel + rasterize ondemand
// ═════════════════════════════════════════════════════════════════════════════

NkSVGImage* NkSVGImage::LoadFromMemory(const uint8* data, usize size) noexcept
{
    if (!data || size < 5) return nullptr;
    const char* xml = reinterpret_cast<const char*>(data);

    // Alloc impl + parse.
    SVGImageImpl* impl = (SVGImageImpl*)NkAlloc(sizeof(SVGImageImpl));
    if (!impl) return nullptr;
    new (impl) SVGImageImpl();
    ParseSVGDocument(xml, size, impl->shapes,
                     impl->vbX, impl->vbY, impl->vbW, impl->vbH,
                     impl->svgW, impl->svgH);

    // Si parsing a echoue (aucun shape ET aucune viewBox), on libere.
    if (impl->shapes.IsEmpty() && impl->vbW <= 0.f && impl->svgW <= 0.f) {
        impl->~SVGImageImpl();
        NkFree(impl);
        return nullptr;
    }

    // Defauts raisonnables si l'auteur du SVG a omis viewBox ou width/height.
    if (impl->vbW <= 0.f) impl->vbW = (impl->svgW > 0.f) ? impl->svgW : 800.f;
    if (impl->vbH <= 0.f) impl->vbH = (impl->svgH > 0.f) ? impl->svgH : 600.f;
    if (impl->svgW <= 0.f) impl->svgW = impl->vbW;
    if (impl->svgH <= 0.f) impl->svgH = impl->vbH;

    // Alloc l'image vectorielle (objet + impl).
    NkSVGImage* svg = (NkSVGImage*)NkAlloc(sizeof(NkSVGImage));
    if (!svg) {
        impl->~SVGImageImpl();
        NkFree(impl);
        return nullptr;
    }
    new (svg) NkSVGImage();
    svg->mImpl = impl;
    return svg;
}

NkSVGImage* NkSVGImage::LoadFromFile(const char* path) noexcept
{
    if (!path) return nullptr;
    NkFile f;
    if (!f.Open(path, NkFileMode::NK_READ)) return nullptr;
    const usize sz = (usize)f.Size();
    if (sz == 0) { f.Close(); return nullptr; }
    uint8* buf = (uint8*)NkAlloc(sz + 1);
    if (!buf) { f.Close(); return nullptr; }
    const usize read = f.Read(buf, sz);
    buf[read] = 0;
    f.Close();
    NkSVGImage* svg = LoadFromMemory(buf, read);
    NkFree(buf);
    return svg;
}

NkImage* NkSVGImage::Rasterize(int32 outW, int32 outH) const noexcept
{
    const SVGImageImpl* impl = (const SVGImageImpl*)mImpl;
    if (!impl) return nullptr;

    // Resolution de la taille de sortie.
    if (outW <= 0 && outH <= 0) {
        outW = (int32)impl->svgW;
        outH = (int32)impl->svgH;
    } else if (outW <= 0) {
        outW = (int32)((float32)outH * impl->vbW / impl->vbH);
    } else if (outH <= 0) {
        outH = (int32)((float32)outW * impl->vbH / impl->vbW);
    }
    if (outW <= 0) outW = 1;
    if (outH <= 0) outH = 1;

    // Mapping viewBox -> taille de sortie.
    const NkSVGTransform mView =
        NkSVGTransform::Scale((float32)outW / impl->vbW, (float32)outH / impl->vbH)
      * NkSVGTransform::Translate(-impl->vbX, -impl->vbY);

    // Alloc image RGBA32 (zero-initialise = fond transparent).
    NkImage* img = NkImage::Alloc(outW, outH, NkImagePixelFormat::NK_RGBA32);
    if (!img) return nullptr;

    // Copie locale des shapes + applique le mapping a leurs points, puis
    // rasterise. On NE modifie PAS l'impl source (pour que Rasterize() puisse
    // etre appele plusieurs fois avec des tailles differentes).
    for (uint32 i = 0; i < impl->shapes.Size(); ++i) {
        const Shape& src = impl->shapes[i];
        // Construit un Shape transforme localement (sans toucher la source).
        Shape local;
        local.style = src.style;
        local.xs.Reserve(src.xs.Size());
        local.ys.Reserve(src.ys.Size());
        for (uint32 k = 0; k < src.xs.Size(); ++k) {
            float32 x = src.xs[k], y = src.ys[k];
            mView.Apply(x, y);
            local.xs.PushBack(x);
            local.ys.PushBack(y);
        }
        for (uint32 k = 0; k < src.contourStart.Size(); ++k) {
            local.contourStart.PushBack(src.contourStart[k]);
            local.contourLen.PushBack(src.contourLen[k]);
        }
        // ── Rasterise le fill (couleur de remplissage) ─────────────────────
        RasterizeShape(*img, local);

        // ── Rasterise le stroke si present ─────────────────────────────────
        // On construit une nouvelle Shape "ruban" autour des contours et on
        // la rasterise comme un fill avec la couleur stroke. strokeWidth est
        // en unites SVG (avant view->out scale) : on le convertit en pixels
        // via la moyenne des facteurs du mView (mView est diagonal en pratique).
        if (!src.style.stroke.none && src.style.strokeWidth > 0.f) {
            const float32 sx = std::sqrt(mView.a * mView.a + mView.b * mView.b);
            const float32 sy = std::sqrt(mView.c * mView.c + mView.d * mView.d);
            const float32 swPx = src.style.strokeWidth * ((sx + sy) * 0.5f);
            if (swPx >= 0.1f) {
                Shape strokeShape;
                strokeShape.style              = src.style;
                // RasterizeShape regarde "fill" -> on copie stroke dedans.
                strokeShape.style.fill         = src.style.stroke;
                strokeShape.style.fillOpacity  = src.style.strokeOpacity;
                strokeShape.style.fillEvenOdd  = false;
                BuildStrokeShape(local, swPx * 0.5f, strokeShape);
                if (strokeShape.contourStart.Size() > 0) {
                    RasterizeShape(*img, strokeShape);
                }
            }
        }
    }
    return img;
}

int32 NkSVGImage::NaturalWidth() const noexcept
{
    const SVGImageImpl* impl = (const SVGImageImpl*)mImpl;
    return impl ? (int32)impl->svgW : 0;
}

int32 NkSVGImage::NaturalHeight() const noexcept
{
    const SVGImageImpl* impl = (const SVGImageImpl*)mImpl;
    return impl ? (int32)impl->svgH : 0;
}

int32 NkSVGImage::ShapeCount() const noexcept
{
    const SVGImageImpl* impl = (const SVGImageImpl*)mImpl;
    return impl ? (int32)impl->shapes.Size() : 0;
}

NkSVGShapeView NkSVGImage::GetShape(int32 idx) const noexcept
{
    NkSVGShapeView v;
    const SVGImageImpl* impl = (const SVGImageImpl*)mImpl;
    if (!impl || idx < 0 || (uint32)idx >= impl->shapes.Size()) return v;
    v.mShapePtr = &impl->shapes[(uint32)idx];
    return v;
}

bool NkSVGImage::TriangulateAll(NkVector<float32>& outXs,
                                NkVector<float32>& outYs,
                                NkVector<uint32>&  outIndices,
                                NkVector<uint32>&  outTriColors) const noexcept
{
    const SVGImageImpl* impl = (const SVGImageImpl*)mImpl;
    if (!impl) return false;
    outXs.Clear(); outYs.Clear(); outIndices.Clear(); outTriColors.Clear();
    for (uint32 i = 0; i < impl->shapes.Size(); ++i) {
        const Shape& sh = impl->shapes[i];
        if (sh.style.fill.none) continue;
        // Triangulate cette shape : chaque triangle produit hérite la couleur fill.
        NkSVGShapeView v;
        v.mShapePtr = &sh;
        const uint32 baseIdx = (uint32)outXs.Size();
        const int32 trisBefore = (int32)(outIndices.Size() / 3);
        v.Triangulate(outXs, outYs, outIndices, baseIdx);
        const int32 trisAfter  = (int32)(outIndices.Size() / 3);
        // Couleur fill premultipliee par opacity * fillOpacity.
        const float32 a = (float32)sh.style.fill.a / 255.f
                        * sh.style.opacity * sh.style.fillOpacity;
        const uint32 col = PackRGBA(sh.style.fill.r, sh.style.fill.g,
                                    sh.style.fill.b, (uint8)(a * 255.f));
        for (int32 k = trisBefore; k < trisAfter; ++k) {
            outTriColors.PushBack(col);
        }
    }
    return true;
}

void NkSVGImage::Free() noexcept
{
    SVGImageImpl* impl = (SVGImageImpl*)mImpl;
    if (impl) {
        impl->~SVGImageImpl();
        NkFree(impl);
        mImpl = nullptr;
    }
    this->~NkSVGImage();
    NkFree(this);
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 11 — NkSVGCodec API publique
// ═════════════════════════════════════════════════════════════════════════════

NkImage* NkSVGCodec::Decode(const uint8* data, usize size,
                            int32 outW, int32 outH) noexcept
{
    // Pipe-through : on parse en NkSVGImage puis on rasterise. Une seule passe
    // si l'appelant n'a pas besoin de conserver la representation vectorielle.
    NkSVGImage* svg = NkSVGImage::LoadFromMemory(data, size);
    if (!svg) return nullptr;
    NkImage* img = svg->Rasterize(outW, outH);
    svg->Free();
    return img;
}

NkImage* NkSVGCodec::DecodeFromFile(const char* path,
                                    int32 outW, int32 outH) noexcept
{
    NkSVGImage* svg = NkSVGImage::LoadFromFile(path);
    if (!svg) return nullptr;
    NkImage* img = svg->Rasterize(outW, outH);
    svg->Free();
    return img;
}

// L'encodage SVG (NkImage -> SVG enrobant un PNG base64) n'est pas prioritaire.
// On le laisse stub pour l'instant ; a implementer si besoin.
bool NkSVGCodec::Encode(const NkImage&, uint8*&, usize&) noexcept
{
    return false;
}

bool NkSVGCodec::EncodeToFile(const NkImage&, const char*) noexcept
{
    return false;
}

}  // namespace nkentseu
